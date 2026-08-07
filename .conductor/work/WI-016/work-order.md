WI-ID: WI-016
类型: test
负责人: worker
依赖: [P0-04 ✅]
范围:
  修改: [tests/CMakeLists.txt]
验收标准:
  - enable_testing() 已存在
  - 7 个单测 + 2 个集成测试全部用 add_test() 注册
  - 集成测试用 EXISTS guard 保护（WI-015 创建源文件后自动生效）
  - ctest --output-on-failure 可发现全部测试
非目标: [不涉及 CI/CD 流水线]
状态: done
