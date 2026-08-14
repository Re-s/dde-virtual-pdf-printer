# 双 Agent 独立安全审查对比报告

> 2026-08-14 | dde-pdf-printer v0.8.3
> 审查方式：主 Agent（Hermes）+ 子 Agent（codex / kimi-k2.7-code，独立上下文，仅见源码 + dfyx 方法论）

## 一、审查设置

| 项 | 主 Agent（Hermes） | 子 Agent（codex） |
| --- | --- | --- |
| 模型 | deepseek-v4-flash-0731 | kimi-k2.7-code |
| 上下文 | 历史会话（含修复过程） | **全新独立**（仅源码 + 方法论，无修复过程信息） |
| 方法论 | dfyx 10 维度 + 手动验证 | dfyx 三层分析法 + D1~D10 |
| 验证方式 | 动态攻击场景实测（/etc 配置、symlink 逃逸） | 只读静态分析 |

## 二、结论对比

| 严重度 | 主 Agent | 子 Agent（codex） |
| --- | --- | --- |
| 高危 | 0（修复前 2 个已修） | **0** |
| 中危 | 0（修复前 2 个已修） | **0** |
| 低危 | 3 | **3** |

## 三、逐项对比

### 双方一致确认安全 ✅
| 面 | 主 Agent | codex | 证据一致性 |
| --- | --- | --- | --- |
| QProcess 全参数化（无 shell 注入） | ✅ | ✅ | printermanager.cpp:52/107/125/153 列表参数 |
| backend `su -c` shlex.quote | ✅ | ✅（低危提示） | ddepdf:108 |
| 文件名 sanitize（防穿越/注入） | ✅ | ✅ | sanitize_filename 白名单 |
| outputDir 家目录约束 + realpath | ✅（已修复并实测） | ✅（确认修复存在） | ddepdf:74-96 双层校验 |
| symlink 逃逸防护 | ✅（已修复并实测） | ✅（确认修复存在） | realpath 最终防线 |
| 无网络外连 / 无密钥 / 无反序列化 | ✅ | ✅ | D4/D6/D7 |
| 二进制加固（Canary/RELRO/FORTIFY/脱敏） | ✅（已修复） | ✅（确认存在） | CMakeLists hardening |
| 维护脚本幂等 / 固定参数 | ✅ | ✅ | postinst/prerm |
| 打印数据流 / PJL 剥离 / copies 校验 | ✅ | ✅ | main() 入口 |

### 双方共识的低危（重叠）⚠️
| 发现 | 主 Agent | codex | 处置 |
| --- | --- | --- | --- |
| TOCTOU 竞态窗口（realpath 校验与写入之间） | 低（毫秒级，本地无利） | 低（建议 O_NOFOLLOW 纵深防御） | **共识**：记录为纵深防御建议，暂不引入复杂度 |
| `su -c` 非 root 兜底分支 | 低（不可达） | 低（不可达） | **共识**：保留，注释说明 |

### 子 Agent 独有发现 🔍
| 发现 | 严重度 | 处置 |
| --- | --- | --- |
| `scripts/collect_forum_posts.py` 含网络请求（开发辅助） | 低 | **已核实**：deb 包不含 scripts/（0 文件），打包脚本不收集——建议已天然满足，无风险 |

### 主 Agent 独有发现（codex 未覆盖）🛠
| 发现 | 严重度 | 说明 |
| --- | --- | --- |
| `debian/deepin-pdf-printer/` 旧名残留（对外违规） | 低 | codex 只读当前代码未查 git 历史/残留，主 Agent 已清理 |
| control 缺 `Recommends: dde-api`（dde-open 依赖） | 低 | 供应链依赖完整性，主 Agent 已补 |
| 二进制路径泄漏（`/home/master/...`） | 低 | codex 确认 file-prefix-map 已启用（修复后状态），未见修复前问题 |

## 四、结论

1. **双审查高度一致**：均判定无高/中危漏洞；低危清单核心重叠（TOCTOU + su -c 兜底）
2. **交叉验证有效**：codex 在无修复过程信息的情况下，独立确认了全部安全修复的最终状态（家目录约束、realpath 防线、hardening flags、参数化调用）——**证明修复真实有效且代码可审计性良好**
3. **差异互补**：codex 补出 scripts/ 打包建议（已核实天然满足）；主 Agent 补出 codex 静态分析看不到的工程问题（残留目录、依赖声明、二进制脱敏）
4. **总体判定**：**通过** ✅ —— 无高/中危；3 项低危均为纵深防御建议，不阻塞发布

## 五、归档

- 子 Agent 独立报告：`docs/security-reviews/independent-audit-codex.md`
- 本对比报告：`docs/security-reviews/comparison-report.md`
- 主 Agent 全量报告：`docs/security-audit.md`
