
//   g++ -std=c++17 bit.cpp -lz -lcrypto -o bit
// zlib, OpenSSL (for SHA-1)
#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <openssl/sha.h>
#include <zlib.h>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// SHA1 of bytes -> hex string
static std::string sha1_hex(const std::vector<unsigned char>& data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(data.data(), data.size(), hash);
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) os << std::setw(2) << (int)hash[i];
    return os.str();
}

static std::vector<unsigned char> sha1_raw(const std::vector<unsigned char>& data) {
    std::vector<unsigned char> out(SHA_DIGEST_LENGTH);
    SHA1(data.data(), data.size(), out.data());
    return out;
}

// zlib compress
static std::vector<unsigned char> zcompress(const std::vector<unsigned char>& in) {
    uLongf out_size = compressBound(in.size());
    std::vector<unsigned char> out(out_size);
    if (compress2(out.data(), &out_size, in.data(), in.size(), Z_BEST_COMPRESSION) != Z_OK) {
        throw std::runtime_error("zlib compress failed");
    }
    out.resize(out_size);
    return out;
}

// zlib uncompress: try to uncompress with doubling buffer
static std::vector<unsigned char> zuncompress(const std::vector<unsigned char>& in) {
    uLongf out_size = in.size() * 4 + 1024; // initial guess
    std::vector<unsigned char> out;
    int status = Z_BUF_ERROR;
    for (int i = 0; i < 8 && status == Z_BUF_ERROR; ++i) {
        out.resize(out_size);
        status = uncompress(out.data(), &out_size, in.data(), in.size());
        if (status == Z_OK) {
            out.resize(out_size);
            return out;
        }
        out_size *= 2;
    }
    if (status != Z_OK) throw std::runtime_error("zlib uncompress failed");
    return out;
}

// JSON save/load for index (map<string,string>)
static void save_index_json(const fs::path& path, const std::map<std::string, std::string>& index) {
    std::ofstream os(path);
    os << "{\n";
    bool first = true;
    for (auto &kv : index) {
        if (!first) os << ",\n";
        first = false;
        // escape backslashes and quotes
        std::string key = kv.first;
        std::string val = kv.second;
        auto esc = [](const std::string &s){ std::string r; for (char c: s){ if (c=='\\') r += "\\\\"; else if (c=='\"') r += "\\\""; else r += c;} return r; };
        os << "  \"" << esc(key) << "\": \"" << esc(val) << "\"";
    }
    os << "\n}\n";
}

static std::map<std::string, std::string> load_index_json(const fs::path& path) {
    std::map<std::string, std::string> index;
    if (!fs::exists(path)) return index;
    std::ifstream is(path);
    std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    // find pairs of "key": "value"
    size_t pos = 0;
    while (true) {
        size_t k1 = content.find('"', pos);
        if (k1 == std::string::npos) break;
        size_t k2 = content.find('"', k1 + 1);
        if (k2 == std::string::npos) break;
        std::string key = content.substr(k1 + 1, k2 - k1 - 1);
        size_t colon = content.find(':', k2);
        if (colon == std::string::npos) break;
        size_t v1 = content.find('"', colon);
        if (v1 == std::string::npos) break;
        size_t v2 = content.find('"', v1 + 1);
        if (v2 == std::string::npos) break;
        std::string val = content.substr(v1 + 1, v2 - v1 - 1);
        index[key] = val;
        pos = v2 + 1;
    }
    return index;
}

// Base class
struct GitObject {
    std::string type;
    std::vector<unsigned char> content; // raw content (uncompressed)
    GitObject(std::string t, std::vector<unsigned char> c): type(std::move(t)), content(std::move(c)) {}
    std::vector<unsigned char> serialize_raw() const {
        std::ostringstream h;
        h << type << " " << content.size() << "\0";
        std::string header = h.str();
        std::vector<unsigned char> out(header.begin(), header.end());
        out.insert(out.end(), content.begin(), content.end());
        return out;
    }
    std::string hash() const {
        auto raw = serialize_raw();
        return sha1_hex(raw);
    }
    std::vector<unsigned char> serialize() const {
        auto raw = serialize_raw();
        return zcompress(raw);
    }
    static GitObject deserialize(const std::vector<unsigned char>& disk_data) {
        auto raw = zuncompress(disk_data);
        auto it = std::find(raw.begin(), raw.end(), 0);
        if (it == raw.end()) throw std::runtime_error("invalid object: missing header null");
        size_t idx = it - raw.begin();
        std::string header(raw.begin(), raw.begin() + idx);
        std::string type;
        size_t sp = header.find(' ');
        if (sp == std::string::npos) throw std::runtime_error("invalid header");
        type = header.substr(0, sp);
        std::vector<unsigned char> content(raw.begin() + idx + 1, raw.end());
        return GitObject(type, content);
    }
};

struct Blob: public GitObject {
    Blob(const std::vector<unsigned char>& c): GitObject("blob", c) {}
};

struct Tree: public GitObject {
    // entries: vector of (mode, name, hash)
    std::vector<std::tuple<std::string, std::string, std::string>> entries;
    Tree() : GitObject("tree", std::vector<unsigned char>{}) {}
    Tree(const std::vector<std::tuple<std::string,std::string,std::string>>& e): GitObject("tree", std::vector<unsigned char>{}), entries(e) {
        content = serialize_entries();
    }
    std::vector<unsigned char> serialize_entries() const {
        std::vector<unsigned char> out;
        // entries sorted by (mode, name)
        auto sorted = entries;
        std::sort(sorted.begin(), sorted.end(), [](auto &a, auto &b){ if (std::get<0>(a)!=std::get<0>(b)) return std::get<0>(a) < std::get<0>(b); return std::get<1>(a) < std::get<1>(b); });
        for (auto &t : sorted) {
            const std::string &mode = std::get<0>(t);
            const std::string &name = std::get<1>(t);
            const std::string &h = std::get<2>(t);
            std::string header = mode + " " + name + '\0';
            out.insert(out.end(), header.begin(), header.end());
            // append raw 20 bytes of hash
            // convert hex to bytes
            if (h.size() != 40) throw std::runtime_error("invalid obj hash in tree");
            for (size_t i = 0; i < 20; ++i) {
                unsigned int byte;
                std::stringstream ss;
                ss << std::hex << h.substr(i*2, 2);
                ss >> byte;
                out.push_back(static_cast<unsigned char>(byte));
            }
        }
        return out;
    }
    void add_entry(const std::string& mode, const std::string& name, const std::string& hash) {
        entries.emplace_back(mode, name, hash);
        content = serialize_entries();
    }
    static Tree from_content(const std::vector<unsigned char>& content_bytes) {
        Tree t;
        size_t i = 0;
        while (i < content_bytes.size()) {
            // find null
            size_t null_idx = i;
            while (null_idx < content_bytes.size() && content_bytes[null_idx] != 0) ++null_idx;
            if (null_idx >= content_bytes.size()) break;
            std::string mode_name((char*)&content_bytes[i], (char*)&content_bytes[null_idx]);
            size_t sp = mode_name.find(' ');
            if (sp==std::string::npos) break;
            std::string mode = mode_name.substr(0,sp);
            std::string name = mode_name.substr(sp+1);
            if (null_idx + 21 > content_bytes.size()) break;
            std::ostringstream hex;
            for (size_t k = 0; k < 20; ++k) {
                unsigned char byte = content_bytes[null_idx + 1 + k];
                hex << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
            }
            std::string obj_hash = hex.str();
            t.entries.emplace_back(mode, name, obj_hash);
            i = null_idx + 21;
        }
        t.content = content_bytes;
        return t;
    }
};

struct Commit: public GitObject {
    std::string tree_hash;
    std::vector<std::string> parent_hashes;
    std::string author;
    std::string committer;
    std::string message;
    long long timestamp;
    Commit(const std::string& tree_h, const std::vector<std::string>& parents, const std::string& a, const std::string& c, const std::string& msg, long long ts = 0)
    : GitObject("commit", std::vector<unsigned char>{}), tree_hash(tree_h), parent_hashes(parents), author(a), committer(c), message(msg) {
        timestamp = ts == 0 ? std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) : ts;
        content = serialize_commit();
    }
    std::vector<unsigned char> serialize_commit() const {
        std::ostringstream os;
        os << "tree " << tree_hash << '\n';
        for (auto &p: parent_hashes) os << "parent " << p << '\n';
        os << "author " << author << " " << timestamp << " +0000\n";
        os << "committer " << committer << " " << timestamp << " +0000\n";
        os << "\n";
        os << message;
        std::string s = os.str();
        return std::vector<unsigned char>(s.begin(), s.end());
    }
    static Commit from_content(const std::vector<unsigned char>& content_bytes) {
        std::string s((char*)content_bytes.data(), content_bytes.size());
        std::istringstream is(s);
        std::string line;
        std::string tree_h;
        std::vector<std::string> parents;
        std::string author;
        std::string committer;
        long long timestamp = 0;
        size_t message_start_pos = 0;
        std::vector<std::string> all_lines;
        while (std::getline(is, line)) {
            all_lines.push_back(line);
        }
        size_t i = 0;
        for (; i < all_lines.size(); ++i) {
            auto &ln = all_lines[i];
            if (ln.rfind("tree ", 0) == 0) tree_h = ln.substr(5);
            else if (ln.rfind("parent ", 0) == 0) parents.push_back(ln.substr(7));
            else if (ln.rfind("author ", 0) == 0) {
                std::string rest = ln.substr(7);
                // split last two tokens as timestamp and tz
                size_t last_sp = rest.find_last_of(' ');
                size_t second_last_sp = rest.find_last_of(' ', last_sp-1);
                if (second_last_sp!=std::string::npos) {
                    author = rest.substr(0, second_last_sp);
                    timestamp = std::stoll(rest.substr(second_last_sp+1));
                } else {
                    author = rest;
                }
            }
            else if (ln.rfind("committer ", 0) == 0) {
                std::string rest = ln.substr(10);
                size_t last_sp = rest.find_last_of(' ');
                size_t second_last_sp = rest.find_last_of(' ', last_sp-1);
                if (second_last_sp!=std::string::npos) {
                    committer = rest.substr(0, second_last_sp);
                } else {
                    committer = rest;
                }
            }
            else if (ln.empty()) {
                message_start_pos = i + 1;
                break;
            }
        }
        std::string message;
        for (size_t j = message_start_pos; j < all_lines.size(); ++j) {
            if (j != message_start_pos) message += "\n";
            message += all_lines[j];
        }
        Commit commit(tree_h, parents, author, committer, message, timestamp);
        return commit;
    }
};

// Repository
struct Repository {
    fs::path path;
    fs::path git_dir;
    fs::path objects_dir;
    fs::path refs_dir;
    fs::path heads_dir;
    fs::path head_file;
    fs::path index_file;

    Repository(const fs::path& p = fs::current_path()) {
        path = fs::absolute(p);
        git_dir = path / ".git";
        objects_dir = git_dir / "objects";
        refs_dir = git_dir / "refs";
        heads_dir = refs_dir / "heads";
        head_file = git_dir / "HEAD";
        index_file = git_dir / "index";
    }

    bool init() {
        if (fs::exists(git_dir)) return false;
        fs::create_directories(objects_dir);
        fs::create_directories(heads_dir);
        std::ofstream(head_file) << "ref: refs/heads/master\n";
        save_index({});
        std::cout << "Initialized empty bit repository in " << git_dir << "\n";
        return true;
    }

    std::string store_object(const GitObject& obj) {
        std::string obj_hash = obj.hash();
        fs::path dir = objects_dir / obj_hash.substr(0,2);
        fs::path file = dir / obj_hash.substr(2);
        if (!fs::exists(file)) {
            fs::create_directories(dir);
            auto data = obj.serialize();
            std::ofstream ofs(file, std::ios::binary);
            ofs.write((char*)data.data(), data.size());
        }
        return obj_hash;
    }

    std::optional<GitObject> load_object(const std::string& obj_hash) {
        if (obj_hash.size() < 3) return std::nullopt;
        fs::path file = objects_dir / obj_hash.substr(0,2) / obj_hash.substr(2);
        if (!fs::exists(file)) return std::nullopt;
        std::ifstream ifs(file, std::ios::binary);
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(ifs)), {});
        return GitObject::deserialize(data);
    }

    std::map<std::string,std::string> load_index() {
        return load_index_json(index_file);
    }

    void save_index(const std::map<std::string,std::string>& index) {
        save_index_json(index_file, index);
    }

    void add_file(const std::string& rel_path) {
        fs::path full = path / rel_path;
        if (!fs::exists(full)) throw std::runtime_error("Path not found: " + rel_path);
        auto data = std::vector<unsigned char>((std::istreambuf_iterator<char>(std::ifstream(full, std::ios::binary))), {});
        Blob blob(data);
        std::string h = store_object(blob);
        auto index = load_index();
        index[rel_path] = h;
        save_index(index);
        std::cout << "Added " << rel_path << "\n";
    }

    void add_directory(const std::string& rel_dir) {
        fs::path full = path / rel_dir;
        if (!fs::exists(full)) throw std::runtime_error("Directory not found: " + rel_dir);
        if (!fs::is_directory(full)) throw std::runtime_error(rel_dir + " is not a directory");
        auto index = load_index();
        size_t added = 0;
        for (auto &p : fs::recursive_directory_iterator(full)) {
            if (!p.is_regular_file()) continue;
            if (p.path().string().find(path.string() + "/.git") != std::string::npos) continue;
            fs::path rel = fs::relative(p.path(), path);
            auto data = std::vector<unsigned char>((std::istreambuf_iterator<char>(std::ifstream(p.path(), std::ios::binary))), {});
            Blob blob(data);
            std::string h = store_object(blob);
            index[rel.string()] = h;
            ++added;
        }
        save_index(index);
        if (added>0) std::cout << "Added " << added << " files from directory " << rel_dir << "\n";
        else std::cout << "Directory " << rel_dir << " already up to date\n";
    }

    void add_path(const std::string& path_s) {
        fs::path full = path / path_s;
        if (!fs::exists(full)) throw std::runtime_error("Path not found: " + path_s);
        if (fs::is_regular_file(full)) add_file(path_s);
        else if (fs::is_directory(full)) add_directory(path_s);
        else throw std::runtime_error(path_s + " is neither file nor directory");
    }

    std::string create_tree_from_index() {
        auto index = load_index();
        if (index.empty()) {
            Tree empty;
            return store_object(empty);
        }
        // Build nested map structure
        struct Node { std::map<std::string, std::string> files; std::map<std::string, Node> dirs; };
        Node root;
        for (auto &kv : index) {
            std::string path_str = kv.first;
            std::string blob_hash = kv.second;
            std::vector<std::string> parts;
            std::istringstream ss(path_str);
            std::string part;
            while (std::getline(ss, part, '/')) parts.push_back(part);
            Node *cur = &root;
            for (size_t i = 0; i < parts.size(); ++i) {
                if (i + 1 == parts.size()) {
                    cur->files[parts[i]] = blob_hash;
                } else {
                    cur = &cur->dirs[parts[i]];
                }
            }
        }
        std::function<std::string(const Node&)> rec = [&](const Node &n) -> std::string {
            Tree t;
            for (auto &f : n.files) t.add_entry("100644", f.first, f.second);
            for (auto &d : n.dirs) {
                std::string subtree_hash = rec(d.second);
                t.add_entry("40000", d.first, subtree_hash);
            }
            return store_object(t);
        };
        return rec(root);
    }

    std::string get_current_branch() {
        if (!fs::exists(head_file)) return "master";
        std::ifstream is(head_file);
        std::string content;
        std::getline(is, content);
        if (content.rfind("ref: refs/heads/", 0) == 0) return content.substr(16);
        return "HEAD";
    }

    std::optional<std::string> get_branch_commit(const std::string& branch) {
        fs::path branch_file = heads_dir / branch;
        if (!fs::exists(branch_file)) return std::nullopt;
        std::string s;
        std::ifstream(branch_file) >> s;
        return s;
    }

    void set_branch_commit(const std::string& branch, const std::string& commit_hash) {
        fs::path branch_file = heads_dir / branch;
        std::ofstream(branch_file) << commit_hash << "\n";
    }

    std::optional<std::string> commit(const std::string& message, const std::string& author = "bit user <user@bit>") {
        std::string tree_hash = create_tree_from_index();
        std::string current_branch = get_current_branch();
        auto parent = get_branch_commit(current_branch);
        std::vector<std::string> parents;
        if (parent) parents.push_back(*parent);
        auto index = load_index();
        if (index.empty()) {
            std::cout << "nothing to commit, working tree clean\n";
            return std::nullopt;
        }
        if (parent) {
            auto parent_obj = load_object(*parent);
            if (parent_obj) {
                if (parent_obj->type == "commit") {
                    Commit parent_commit = Commit::from_content(parent_obj->content);
                    if (parent_commit.tree_hash == tree_hash) {
                        std::cout << "nothing to commit, working tree clean\n";
                        return std::nullopt;
                    }
                }
            }
        }
        Commit c(tree_hash, parents, author, author, message);
        std::string commit_hash = store_object(c);
        set_branch_commit(current_branch, commit_hash);
        save_index({});
        std::cout << "Created commit " << commit_hash << " on branch " << current_branch << "\n";
        return commit_hash;
    }

    std::set<std::string> get_files_from_tree_recursive(const std::string& tree_hash, const std::string& prefix = "") {
        std::set<std::string> files;
        auto obj = load_object(tree_hash);
        if (!obj) return files;
        if (obj->type != "tree") return files;
        Tree t = Tree::from_content(obj->content);
        for (auto &e : t.entries) {
            std::string mode = std::get<0>(e);
            std::string name = std::get<1>(e);
            std::string h = std::get<2>(e);
            std::string full = prefix + name;
            if (mode.rfind("100", 0) == 0) files.insert(full);
            else if (mode.rfind("400", 0) == 0) {
                auto sub = get_files_from_tree_recursive(h, full + "/");
                files.insert(sub.begin(), sub.end());
            }
        }
        return files;
    }

    void restore_tree(const std::string& tree_hash, const fs::path& target) {
        auto obj = load_object(tree_hash);
        if (!obj) return;
        Tree t = Tree::from_content(obj->content);
        for (auto &e : t.entries) {
            std::string mode = std::get<0>(e);
            std::string name = std::get<1>(e);
            std::string h = std::get<2>(e);
            fs::path file = target / name;
            if (mode.rfind("100",0) == 0) {
                auto blob_obj = load_object(h);
                if (!blob_obj) continue;
                std::ofstream ofs(file, std::ios::binary);
                ofs.write((char*)blob_obj->content.data(), blob_obj->content.size());
            } else if (mode.rfind("400",0) == 0) {
                fs::create_directories(file);
                restore_tree(h, file);
            }
        }
    }

    void restore_working_directory(const std::string& branch, const std::set<std::string>& files_to_clear) {
        auto target_commit = get_branch_commit(branch);
        if (!target_commit) return;
        // remove previous files
        for (auto &rel : files_to_clear) {
            fs::path f = path / rel;
            try { if (fs::exists(f) && fs::is_regular_file(f)) fs::remove(f); } catch (...) {}
        }
        auto obj = load_object(*target_commit);
        if (!obj) return;
        Commit target = Commit::from_content(obj->content);
        if (!target.tree_hash.empty()) {
            restore_tree(target.tree_hash, path);
        }
        save_index({});
    }

    void checkout(const std::string& branch, bool create_branch) {
        std::string previous_branch = get_current_branch();
        std::set<std::string> files_to_clear;
        auto prev_commit = get_branch_commit(previous_branch);
        if (prev_commit) {
            auto prev_obj = load_object(*prev_commit);
            if (prev_obj && prev_obj->type == "commit") {
                Commit prevc = Commit::from_content(prev_obj->content);
                if (!prevc.tree_hash.empty()) files_to_clear = get_files_from_tree_recursive(prevc.tree_hash);
            }
        }
        fs::path branch_file = heads_dir / branch;
        if (!fs::exists(branch_file)) {
            if (create_branch) {
                if (prev_commit) {
                    set_branch_commit(branch, *prev_commit);
                    std::cout << "Created new branch " << branch << "\n";
                } else {
                    std::cout << "No commits yet, cannot create a branch\n";
                    return;
                }
            } else {
                std::cout << "Branch '" << branch << "' not found.\n";
                std::cout << "Use 'bit checkout -b <branch>' to create and switch to a new branch.\n";
                return;
            }
        }
        std::ofstream(head_file) << "ref: refs/heads/" << branch << "\n";
        restore_working_directory(branch, files_to_clear);
        std::cout << "Switched to branch " << branch << "\n";
    }

    void branch(const std::optional<std::string>& name, bool delete_flag) {
        if (delete_flag && name) {
            fs::path f = heads_dir / *name;
            if (fs::exists(f)) { fs::remove(f); std::cout << "Deleted branch " << *name << "\n"; }
            else std::cout << "Branch " << *name << " not found\n";
            return;
        }
        std::string current = get_current_branch();
        if (name) {
            auto cur_commit = get_branch_commit(current);
            if (cur_commit) {
                set_branch_commit(*name, *cur_commit);
                std::cout << "Created branch " << *name << "\n";
            } else {
                std::cout << "No commits yet, cannot create a new branch\n";
            }
            return;
        }
        // list branches
        for (auto &entry : fs::directory_iterator(heads_dir)) {
            if (!entry.is_regular_file()) continue;
            std::string b = entry.path().filename().string();
            std::string marker = (b == current) ? "* " : "  ";
            std::cout << marker << b << "\n";
        }
    }

    void log(int max_count = 10) {
        std::string branch = get_current_branch();
        auto commit = get_branch_commit(branch);
        if (!commit) { std::cout << "No commits yet!\n"; return; }
        int count = 0;
        auto cur = commit;
        while (cur && count < max_count) {
            auto obj = load_object(*cur);
            if (!obj) break;
            if (obj->type != "commit") break;
            Commit c = Commit::from_content(obj->content);
            std::time_t t = (std::time_t)c.timestamp;
            std::cout << "commit " << *cur << "\n";
            std::cout << "Author: " << c.author << "\n";
            std::cout << "Date: " << std::asctime(std::gmtime(&t));
            std::cout << "\n    " << c.message << "\n\n";
            if (!c.parent_hashes.empty()) cur = c.parent_hashes[0]; else cur.reset();
            ++count;
        }
    }

    std::map<std::string,std::string> build_index_from_tree(const std::string& tree_hash, const std::string& prefix = "") {
        std::map<std::string,std::string> index;
        auto obj = load_object(tree_hash);
        if (!obj) return index;
        if (obj->type != "tree") return index;
        Tree t = Tree::from_content(obj->content);
        for (auto &e : t.entries) {
            std::string mode = std::get<0>(e);
            std::string name = std::get<1>(e);
            std::string h = std::get<2>(e);
            std::string full = prefix + name;
            if (mode.rfind("100",0)==0) index[full] = h;
            else if (mode.rfind("400",0) == 0) {
                auto sub = build_index_from_tree(h, full + "/");
                index.insert(sub.begin(), sub.end());
            }
        }
        return index;
    }

    std::vector<fs::path> get_all_files() {
        std::vector<fs::path> files;
        for (auto &p : fs::recursive_directory_iterator(path)) {
            if (!p.is_regular_file()) continue;
            if (p.path().string().find((path / ".git").string()) != std::string::npos) continue;
            files.push_back(p.path());
        }
        return files;
    }

    void status() {
        std::string current_branch = get_current_branch();
        std::cout << "On branch " << current_branch << "\n";
        auto index = load_index();
        auto current_commit = get_branch_commit(current_branch);
        std::map<std::string,std::string> last_index_files;
        if (current_commit) {
            auto obj = load_object(*current_commit);
            if (obj && obj->type == "commit") {
                Commit c = Commit::from_content(obj->content);
                if (!c.tree_hash.empty()) last_index_files = build_index_from_tree(c.tree_hash);
            }
        }
        // working files
        std::map<std::string,std::string> working_files;
        for (auto &p : get_all_files()) {
            std::string rel = fs::relative(p, path).string();
            std::ifstream ifs(p, std::ios::binary);
            std::vector<unsigned char> data((std::istreambuf_iterator<char>(ifs)), {});
            Blob b(data);
            working_files[rel] = b.hash();
        }
        std::vector<std::pair<std::string,std::string>> staged;
        std::vector<std::string> unstaged;
        std::vector<std::string> untracked;
        std::vector<std::string> deleted;
        // staged
        std::set<std::string> all_paths;
        for (auto &kv : index) all_paths.insert(kv.first);
        for (auto &kv : last_index_files) all_paths.insert(kv.first);
        for (auto &p : all_paths) {
            auto idx = index.find(p);
            auto lst = last_index_files.find(p);
            if (idx != index.end() && lst == last_index_files.end()) staged.emplace_back("new file", p);
            else if (idx != index.end() && lst != last_index_files.end() && idx->second != lst->second) staged.emplace_back("modified", p);
        }
        if (!staged.empty()) {
            std::cout << "\nChanges to be committed:\n";
            for (auto &s : staged) std::cout << "   " << s.first << ": " << s.second << "\n";
        }
        for (auto &wf : working_files) {
            auto it = index.find(wf.first);
            if (it != index.end()) {
                if (it->second != wf.second) unstaged.push_back(wf.first);
            }
        }
        if (!unstaged.empty()) {
            std::cout << "\nChanges not staged for commit:\n";
            for (auto &u : unstaged) std::cout << "   modified: " << u << "\n";
        }
        for (auto &wf : working_files) {
            if (index.find(wf.first) == index.end() && last_index_files.find(wf.first) == last_index_files.end()) untracked.push_back(wf.first);
        }
        if (!untracked.empty()) {
            std::cout << "\nUntracked files:\n";
            for (auto &u : untracked) std::cout << "   " << u << "\n";
        }
        for (auto &kv : index) {
            if (working_files.find(kv.first) == working_files.end()) deleted.push_back(kv.first);
        }
        if (!deleted.empty()) {
            std::cout << "\nDeleted files:\n";
            for (auto &d : deleted) std::cout << "   deleted: " << d << "\n";
        }
        if (staged.empty() && unstaged.empty() && deleted.empty() && untracked.empty()) std::cout << "\nnothing to commit, working tree clean\n";
    }
};

// Parse arguments and dispatch commands
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "bit - a tiny git-like tool\n";
        std::cout << "Usage: bit <command> [options]\n";
        std::cout << "Commands: init, add, commit, checkout, branch, log, status\n";
        return 0;
    }
    std::string cmd = argv[1];
    Repository repo;
    try {
        if (cmd == "init") {
            if (!repo.init()) std::cout << "Repository already exists\n";
        } else if (cmd == "add") {
            if (!fs::exists(repo.git_dir)) { std::cout << "Not a bit repository\n"; return 1; }
            if (argc < 3) { std::cout << "Usage: bit add <paths...>\n"; return 1; }
            for (int i = 2; i < argc; ++i) repo.add_path(argv[i]);
        } else if (cmd == "commit") {
            if (!fs::exists(repo.git_dir)) { std::cout << "Not a bit repository\n"; return 1; }
            std::string message;
            std::string author = "bit user <user@bit>";
            for (int i = 2; i < argc; ++i) {
                std::string a = argv[i];
                if (a == "-m" && i+1 < argc) { message = argv[++i]; }
                else if (a == "--author" && i+1 < argc) { author = argv[++i]; }
            }
            if (message.empty()) { std::cout << "Commit message required: -m \"msg\"\n"; return 1; }
            repo.commit(message, author);
        } else if (cmd == "checkout") {
            if (!fs::exists(repo.git_dir)) { std::cout << "Not a bit repository\n"; return 1; }
            bool create = false;
            std::string branch;
            for (int i = 2; i < argc; ++i) {
                std::string a = argv[i];
                if (a == "-b") create = true;
                else branch = a;
            }
            if (branch.empty()) { std::cout << "Usage: bit checkout [-b] <branch>\n"; return 1; }
            repo.checkout(branch, create);
        } else if (cmd == "branch") {
            if (!fs::exists(repo.git_dir)) { std::cout << "Not a bit repository\n"; return 1; }
            bool del = false;
            std::optional<std::string> name;
            for (int i = 2; i < argc; ++i) {
                std::string a = argv[i];
                if (a == "-d") del = true;
                else name = a;
            }
            repo.branch(name, del);
        } else if (cmd == "log") {
            if (!fs::exists(repo.git_dir)) { std::cout << "Not a bit repository\n"; return 1; }
            int n = 10;
            for (int i = 2; i < argc; ++i) { std::string a = argv[i]; if (a == "-n" && i+1<argc) { n = std::stoi(argv[++i]); } }
            repo.log(n);
        } else if (cmd == "status") {
            if (!fs::exists(repo.git_dir)) { std::cout << "Not a bit repository\n"; return 1; }
            repo.status();
        } else {
            std::cout << "Unknown command: " << cmd << "\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
