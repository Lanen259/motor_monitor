# Git 工作流 (Git Workflow)

## 目标

建立高效的 Git 分支管理、提交规范和 CI/CD 流程，确保 Motor Monitor 项目的协作开发有序、代码质量可控、版本发布可追溯。通过 Conventional Commits 和 Semantic Versioning 实现自动化 changelog 生成和版本管理。

---

## 设计原则

1. **主干稳定**：`main` 分支始终保持可发布状态，任何时刻都能从 `main` 拉取并构建出可用的发布版本。
2. **增量集成**：功能开发在 `feature/*` 分支进行，通过 PR 合并到 `develop`，避免直接提交到 `main`。
3. **提交可追溯**：每条提交遵循 Conventional Commits 格式，可由工具自动生成 changelog。
4. **质量门禁**：CI 流水线在合并前自动执行构建、测试、静态分析，确保代码质量。
5. **语义化版本**：版本号严格遵循 MAJOR.MINOR.PATCH，通过提交类型自动决定版本递增。

---

## 类/模块关系

### Git 分支关系图

```
                    ┌──────────────────────────────────┐
                    │            main (stable)          │
                    │  - 只接受 release/hotfix 合并     │
                    │  - 每个 commit 对应一个发布版本    │
                    │  - 保护分支，禁止直接推送         │
                    └──────────┬───────────────────────┘
                               │ merge (release)
                               │ merge (hotfix, 双向)
                    ┌──────────▼───────────────────────┐
                    │         develop (integration)     │
                    │  - 日常开发集成分支               │
                    │  - 接受 feature/fix/docs 合并     │
                    │  - 保护分支，禁止直接推送         │
                    └──────────┬───────────────────────┘
                               │ merge / rebase
               ┌───────────────┼───────────────┐
               ▼               ▼               ▼
        ┌──────────┐   ┌──────────┐   ┌──────────────┐
        │ feature/ │   │  fix/    │   │  refactor/   │
        │   *      │   │   *      │   │     *        │
        └──────────┘   └──────────┘   └──────────────┘
        - 从 develop 创建             - 合并回 develop
        - 定期 rebase develop         - 合并后删除
```

### CI/CD 模块关系

```
┌──────────────────────────────────────────────────────────────┐
│                      CI Pipeline                              │
│                                                              │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌───────────┐ │
│  │  Build   │──▶│   Lint   │──▶│   Test   │──▶│  Package  │ │
│  │  Module  │   │  Module  │   │  Module  │   │  Module   │ │
│  └────┬─────┘   └────┬─────┘   └────┬─────┘   └─────┬─────┘ │
│       │              │              │               │        │
│       ▼              ▼              ▼               ▼        │
│  CMake + GCC    clang-format    Unit Tests      MSI/DMG/    │
│  /MSVC/Clang    clang-tidy      Coverage        AppImage    │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │               Notification Module                     │   │
│  │  企业微信 / 飞书 / 钉钉 / Email                       │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

### 版本管理模块

```
Conventional Commits
  │
  ▼
semantic-release / standard-version
  │
  ├─ 解析提交历史 → 确定版本号递增 (MAJOR/MINOR/PATCH)
  ├─ 生成 CHANGELOG.md
  ├─ 创建 Git Tag
  └─ 创建 GitHub Release
```

---

## 数据流

### 代码变更流

```
开发者工作流：
  1. git checkout -b feature/xxx develop
  2. 编写代码 + 本地测试
  3. git add + git commit (遵循 Conventional Commits)
  4. git rebase develop (同步最新变更)
  5. git push origin feature/xxx
  6. 创建 Pull Request
  7. 触发 CI Pipeline
  8. Code Review
  9. 合并到 develop
  10. 删除 feature 分支

数据流视角：
  feature 分支 ──PR──▶ CI 检查 ──▶ Review ──▶ develop ──▶ release ──▶ main
```

### CI 流水线数据流

```
PR 创建/更新
  │
  ▼
Build Job
  │ 产出: 编译产物、编译日志
  ▼
Lint Job
  │ 产出: clang-format 检查结果、clang-tidy 报告
  ▼
Test Job
  │ 产出: 测试结果 (JUnit XML)、覆盖率报告 (lcov)
  ▼
Integration Job
  │ 产出: 集成测试结果
  ▼
Status Check → GitHub PR Status (✅ / ❌)
  │
  ▼
(合并后) Deploy Job
  │ 产出: 安装包、Docker 镜像
  ▼
Release Job
  │ 产出: CHANGELOG, Git Tag, GitHub Release
```

### 版本发布数据流

```
develop 分支
  │
  ▼
创建 release/vX.Y.Z 分支
  │
  ├─ 更新版本号 (CMakeLists.txt, 关于对话框)
  ├─ 更新 CHANGELOG.md
  ├─ 最终测试和 Bug 修复
  │
  ▼
创建 RC Tag (vX.Y.Z-rc.N)
  │
  ▼
RC 测试通过
  │
  ├─▶ 合并到 main
  │     ├─ 创建正式 Tag (vX.Y.Z)
  │     └─ 触发 Deploy Pipeline
  │
  └─▶ 合并回 develop (同步 release 期间的修复)
        │
        └─ 删除 release 分支
```

---

## API 接口规划

### Git Hooks 接口

```bash
# .git/hooks/pre-commit
#!/bin/bash
# 提交前自动运行 clang-format 检查
STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(h|cpp)$')
if [ -n "$STAGED_FILES" ]; then
    clang-format --dry-run --Werror $STAGED_FILES
    if [ $? -ne 0 ]; then
        echo "❌ clang-format 检查失败，请运行 clang-format -i 格式化代码"
        exit 1
    fi
fi
```

```bash
# .git/hooks/commit-msg
#!/bin/bash
# 验证提交消息格式 (Conventional Commits)
COMMIT_MSG=$(cat "$1")
PATTERN='^(feat|fix|docs|style|refactor|perf|test|chore|ci|revert)(\([a-z-]+\))?!?: .+'
if ! echo "$COMMIT_MSG" | grep -qE "$PATTERN"; then
    echo "❌ 提交消息不符合 Conventional Commits 格式"
    echo "   格式: type(scope): description"
    echo "   示例: feat(protocol): add Modbus RTU read holding registers function"
    exit 1
fi
```

### 分支管理命令

```bash
# 创建功能分支
git checkout -b feature/<功能名> develop

# 创建修复分支
git checkout -b fix/<问题描述> develop

# 创建紧急修复分支
git checkout -b hotfix/<问题描述> main

# 创建发布分支
git checkout -b release/v<MAJOR>.<MINOR>.<PATCH> develop

# 同步 develop 最新变更
git rebase develop

# 合并到 develop (通过 PR)
# 在 GitHub/GitLab 上创建 Pull Request / Merge Request
```

### CI 配置接口 (GitHub Actions)

```yaml
# .github/workflows/ci.yml 关键事件触发
on:
  pull_request:          # PR 触发：Build + Lint + Test
    branches: [develop, main]
  push:                  # 推送触发：完整流水线
    branches: [develop, main]
    tags: ['v*']         # Tag 触发：构建发布包
```

### 版本自动化接口

```json
// .versionrc.json (standard-version 配置)
{
  "types": [
    {"type": "feat", "section": "✨ Features"},
    {"type": "fix", "section": "🐛 Bug Fixes"},
    {"type": "perf", "section": "⚡ Performance Improvements"},
    {"type": "refactor", "section": "♻️ Code Refactoring"},
    {"type": "docs", "section": "📝 Documentation"},
    {"type": "test", "section": "✅ Tests"},
    {"type": "chore", "section": "🔧 Maintenance"},
    {"type": "ci", "section": "👷 CI/CD"}
  ],
  "releaseCommitMessageFormat": "chore(release): v{{currentTag}}"
}
```

---

## 分支策略

### 分支模型

```
main (stable)
  │
  ├── v1.0.0 (tag)
  ├── v1.1.0 (tag)
  ├── v2.0.0 (tag)
  │
  └── develop (integration)
        │
        ├── feature/core-logger          ──▶ PR ──▶ merge
        ├── feature/modbus-protocol      ──▶ PR ──▶ merge
        ├── feature/plugin-system        ──▶ PR ──▶ merge
        ├── fix/logger-crash-on-shutdown ──▶ PR ──▶ merge
        ├── docs/api-documentation       ──▶ PR ──▶ merge
        ├── refactor/thread-model        ──▶ PR ──▶ merge
        │
        └── release/v1.0.0
              │
              ├── v1.0.0-rc.1 (tag)
              └── merge back to main + develop
```

### 分支命名规范

| 分支类型 | 命名格式 | 示例 | 说明 |
|----------|----------|------|------|
| 功能开发 | `feature/<描述>` | `feature/modbus-protocol` | 新功能开发 |
| Bug 修复 | `fix/<描述>` | `fix/logger-crash` | 非紧急 Bug 修复 |
| 紧急修复 | `hotfix/<描述>` | `hotfix/critical-crash` | 从 main 分支的紧急修复 |
| 文档 | `docs/<描述>` | `docs/api-reference` | 纯文档变更 |
| 重构 | `refactor/<描述>` | `refactor/thread-model` | 无功能变更的重构 |
| 发布准备 | `release/<版本>` | `release/v1.0.0` | 发布候选版本 |
| 实验 | `experiment/<描述>` | `experiment/new-algorithm` | 实验性代码 |

### 分支生命周期

```
1. 从 develop 创建 feature 分支
   git checkout -b feature/modbus-protocol develop

2. 在 feature 分支上开发，定期 rebase develop
   git rebase develop

3. 推送并创建 Pull Request
   git push origin feature/modbus-protocol

4. 通过 Code Review 和 CI 后合并到 develop
   (Squash and Merge 或 Merge Commit)

5. 删除 feature 分支
   git branch -d feature/modbus-protocol
```

---

## 提交规范 (Conventional Commits)

### 格式

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

### Type 类型

| Type | 说明 | 版本影响 |
|------|------|----------|
| `feat` | 新功能 | MINOR++ |
| `fix` | Bug 修复 | PATCH++ |
| `docs` | 文档变更 | - |
| `style` | 代码格式（不影响功能） | - |
| `refactor` | 重构（无功能变更） | - |
| `perf` | 性能优化 | PATCH++ |
| `test` | 添加或修改测试 | - |
| `chore` | 构建/工具/依赖变更 | - |
| `ci` | CI 配置变更 | - |
| `revert` | 回退之前的提交 | - |

### Scope 范围

| Scope | 说明 |
|-------|------|
| `core` | 核心框架 |
| `device` | 设备抽象层 |
| `protocol` | 通信协议 |
| `ui` | 用户界面 |
| `logger` | 日志系统 |
| `plugin` | 插件系统 |
| `automation` | 自动化测试 |
| `data` | 数据管理 |
| `build` | 构建系统 |
| `docs` | 文档 |

### 提交示例

```
feat(protocol): add Modbus RTU read holding registers function

Implemented readHoldingRegisters() with support for:
- Single register read
- Multi-register burst read (up to 125 registers)
- Automatic CRC16 calculation and verification
- Timeout handling with configurable retry

Closes #42
```

```
fix(logger): resolve crash when FileSink shuts down during flush

The crash occurred due to a race condition between the flush timer
and the shutdown sequence. Fixed by adding a shutdown flag and
cancelling the timer before closing file handles.

Fixes #128
```

```
refactor(device): extract MotorConfig from MotorController

Moved configuration parsing logic into a separate MotorConfig struct
to improve separation of concerns and enable unit testing of config
validation independently from the controller.

BREAKING CHANGE: MotorController constructor now takes MotorConfig
instead of individual parameters.
```

### Breaking Changes

- 在 footer 中添加 `BREAKING CHANGE: <description>`
- 或在 type/scope 后添加 `!`：`feat(protocol)!: change API signature`

---

## Pull Request 流程

### PR 创建

```
1. 确保分支已 rebase 到最新 develop
2. 推送分支到远程
3. 创建 PR，填写模板
```

### PR 模板

```markdown
## 概述
简要描述本次变更的内容和目的。

## 变更类型
- [ ] 新功能 (feat)
- [ ] Bug 修复 (fix)
- [ ] 重构 (refactor)
- [ ] 文档 (docs)
- [ ] 其他: _______

## 关联 Issue
Closes #XXX

## 变更详情
详细描述变更内容，包括技术决策和实现方式。

## 测试计划
- [ ] 单元测试已添加/更新
- [ ] 集成测试通过
- [ ] 手动测试步骤：
  1. ...
  2. ...

## 截图 (如有 UI 变更)
(粘贴截图)

## 检查清单
- [ ] 代码符合编码规范
- [ ] 通过了 clang-format 和 clang-tidy
- [ ] 添加了必要的注释
- [ ] 更新了相关文档
- [ ] 无 breaking change 或已明确标注
```

### Code Review 要求

- 至少 1 位 Reviewer 批准
- 所有 CI 检查通过
- 无未解决的 Review 评论
- 关键模块（core, protocol, plugin）需要 2 位 Reviewer 批准

### 合并策略

| 分支类型 | 合并策略 | 说明 |
|----------|----------|------|
| `feature/*` → `develop` | Squash and Merge | 保持 develop 历史清晰 |
| `release/*` → `main` | Merge Commit | 保留完整发布历史 |
| `hotfix/*` → `main` | Merge Commit | 保留修复历史 |
| `main` → `develop` | Merge Commit | 同步 hotfix/release 变更 |

---

## CI 流水线

### 流水线阶段

```
┌─────────────────────────────────────────────────────────────────┐
│                        CI Pipeline                               │
│                                                                  │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────────┐ │
│  │  Build   │──▶│  Lint    │──▶│   Test   │──▶│  Integration │ │
│  │          │   │          │   │          │   │    Test      │ │
│  └──────────┘   └──────────┘   └──────────┘   └──────────────┘ │
│       │              │              │                │           │
│       ▼              ▼              ▼                ▼           │
│  CMake build    clang-format   Unit Tests       E2E Tests       │
│  (Debug+Rel)    clang-tidy     Coverage         (selected)      │
│                                                                  │
│  PR 触发: 全部阶段                                              │
│  main 推送: 全部阶段 + Package + Deploy                         │
└─────────────────────────────────────────────────────────────────┘
```

### 触发条件

| 事件 | 触发阶段 |
|------|----------|
| PR 创建/更新 | Build → Lint → Test → Integration (smoke) |
| PR 合并到 develop | Build → Lint → Test → Integration (full) |
| 推送到 main | Build → Lint → Test → Integration → Package → Deploy |
| 创建 tag | Build → Package → Deploy |

### CI 配置文件 (GitHub Actions 示例)

```yaml
name: CI

on:
  pull_request:
    branches: [develop, main]
  push:
    branches: [develop, main]
    tags: ['v*']

jobs:
  build:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-22.04, windows-2022]
        build_type: [Debug, Release]
    steps:
      - uses: actions/checkout@v4
      - name: Configure CMake
        run: cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}
      - name: Build
        run: cmake --build build --parallel

  lint:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      - name: clang-format
        run: find src -name '*.h' -o -name '*.cpp' | xargs clang-format --dry-run --Werror
      - name: clang-tidy
        run: cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && run-clang-tidy -p build

  test:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      - name: Build & Test
        run: cmake -B build -DBUILD_TESTING=ON && cmake --build build && ctest --test-dir build --output-on-failure
      - name: Coverage
        run: ... # lcov + upload to Codecov

  integration:
    needs: [build, lint, test]
    runs-on: ubuntu-22.04
    steps:
      - name: Integration Tests
        run: ... # 端到端测试
```

---

## 版本管理

### 语义化版本 (Semantic Versioning)

```
MAJOR.MINOR.PATCH

MAJOR: 不兼容的 API 变更 (BREAKING CHANGE)
MINOR: 向后兼容的功能新增 (feat)
PATCH: 向后兼容的 Bug 修复 (fix)
```

### 版本号递增规则

| 提交类型 | 版本递增 | 示例 |
|----------|----------|------|
| `fix: ...` | PATCH | 1.0.0 → 1.0.1 |
| `feat: ...` | MINOR | 1.0.1 → 1.1.0 |
| `feat!: ...` 或 `BREAKING CHANGE` | MAJOR | 1.1.0 → 2.0.0 |

### 发布流程

```
1. 从 develop 创建 release 分支
   git checkout -b release/v1.0.0 develop

2. 在 release 分支上：
   - 更新 CHANGELOG.md
   - 更新版本号（CMakeLists.txt, 关于对话框）
   - 最终测试和 Bug 修复

3. 创建 Release Candidate
   git tag v1.0.0-rc.1
   git push origin v1.0.0-rc.1

4. RC 测试通过后，合并到 main
   git checkout main
   git merge --no-ff release/v1.0.0
   git tag v1.0.0
   git push origin main --tags

5. 合并回 develop
   git checkout develop
   git merge --no-ff release/v1.0.0

6. 删除 release 分支
   git branch -d release/v1.0.0
```

### 自动 Changelog 生成

使用 `standard-version` 或 `semantic-release` 工具根据 Conventional Commits 自动生成：

```markdown
# Changelog

## [1.1.0] - 2026-08-05

### Added
- feat(protocol): add Modbus RTU read holding registers function (#42)
- feat(ui): add motor speed real-time waveform panel (#56)

### Fixed
- fix(logger): resolve crash when FileSink shuts down during flush (#128)
- fix(device): correct CRC calculation for multi-byte responses (#135)

### Changed
- refactor(device): extract MotorConfig from MotorController (#150)
```

---

## 代码审查检查清单

### 功能性
- [ ] 代码实现了预期功能
- [ ] 边界条件和异常情况已处理
- [ ] 错误消息清晰、有意义

### 代码质量
- [ ] 命名符合编码规范
- [ ] 无重复代码
- [ ] 函数长度合理（< 50 行）
- [ ] 复杂逻辑有注释
- [ ] 无 Magic Number

### 测试
- [ ] 单元测试覆盖新功能
- [ ] 测试覆盖边界条件
- [ ] 所有现有测试通过

### 安全
- [ ] 无硬编码密钥/密码
- [ ] 输入验证充分
- [ ] 无 SQL 注入风险（如适用）
- [ ] 权限检查到位

### 性能
- [ ] 无明显的性能问题
- [ ] 热路径无不必要的内存分配
- [ ] 大数据量操作有合理的上限

### 文档
- [ ] API 变更已更新文档
- [ ] 新增配置项已说明
- [ ] Breaking Change 已标注

---

## 后续实现注意事项

1. **Git Hooks**：配置 `pre-commit` 钩子运行 clang-format 和基本检查，`commit-msg` 钩子验证 Conventional Commits 格式。

2. **分支保护**：`main` 和 `develop` 分支启用保护规则：禁止直接推送、要求 PR 审查、要求 CI 通过。

3. **CI 缓存**：配置 CMake 构建缓存和 ccache，加速 CI 构建时间。

4. **通知集成**：PR 创建/审查请求/合并失败时通过企业微信/飞书/钉钉通知相关人员。

5. **Release 自动化**：使用 GitHub Actions 或 GitLab CI 自动构建安装包（Windows MSI, Linux AppImage, macOS DMG）。

6. **Changelog 自动化**：集成 `semantic-release` 或 `standard-version`，在发布时自动生成 CHANGELOG.md 并创建 GitHub Release。

7. **回滚策略**：定义紧急回滚流程：发现问题 → 确认 → 回滚到上一个稳定 tag → 创建 hotfix 分支 → 修复 → 重新发布。

8. **长期支持分支**：对于需要长期维护的版本，从 `main` 创建 `lts/v1.x` 分支，仅合并关键修复。