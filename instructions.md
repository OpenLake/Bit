 # Repository Templates Instructions

This repository contains standardized templates for GitHub/GitLab workflows. Use these files to maintain consistency across projects.

## Available Templates

| File                 | Purpose                                                                 |
|----------------------|-------------------------------------------------------------------------|
| `PULL_REQUEST.md`    | Template for pull request descriptions (auto-fills new PRs)             |
| `ISSUE_TEMPLATE.md`  | Template for bug reports/feature requests                               |
| `CONTRIBUTING.md`    | Guidelines for contributors (code style, setup, etc.)                   |
| `MAINTAINERS.md`     | List of project maintainers and review process                          |
| `dependabot.yml`     | Configuration for automated dependency updates                          |

## How to Use

1. **For New Repositories**:  
   Copy these files to:
   - GitHub: `.github/` directory  
   - GitLab: `.gitlab/` directory  

2. **Customization**:  
   Edit templates to match your project’s needs (e.g., update labels in `PULL_REQUEST.md`).

3. **Automation**:  
   - GitHub: Files in `.github/` work automatically.  
   - GitLab: Merge request templates need `.gitlab/merge_request_templates/`.  

## Updating Templates
- Submit changes via PR (use the `PULL_REQUEST.md` template).  
- Tag maintainers (`MAINTAINERS.md`) for review.  

## Need Help?
Open an issue using the `ISSUE_TEMPLATE.md` format!