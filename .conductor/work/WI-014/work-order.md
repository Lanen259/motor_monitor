WI-ID: WI-014
类型: feature
负责人: worker
依赖: [WI-007 ✅]
范围:
  修改: [TestRunner.h/cpp]
  新增: [ReportGenerator.h/cpp]
验收标准:
  - generateHtml(): 完整 HTML (CSS/汇总卡片/步骤表格/彩色行/错误横幅)
  - generateCsv(): CSV (Step#/Type/Description/Status/DurationMs/ErrorMsg)
  - TestRunner 执行完成自动调用生成 HTML+CSV 到 ./reports/
  - 文件名: report_YYYYMMDD_HHmmss.html / report_YYYYMMDD_HHmmss.csv
  - reportGenerated 信号
非目标: [不涉及邮件/网络发送]
状态: done
