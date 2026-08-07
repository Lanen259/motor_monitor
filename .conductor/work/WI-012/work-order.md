WI-ID: WI-012
类型: feature
负责人: worker
依赖: [WI-006 ✅]
范围:
  修改: [mainwindow.h/cpp]
  新增: [DynamicWidgetFactory.h/cpp, WidgetBindingManager.h/cpp]
验收标准:
  - DynamicWidgetFactory::createWidget("button"|"slider"|"input") 返回对应 QWidget
  - WidgetBindingManager::bind(widget, command) 绑定命令，{value} 替换
  - Button 点击/slider 拖动/input 回车 → 执行命令
  - "+" 工具栏按钮 → 创建对话框 (类型/标签/命令)
  - 右键菜单 Edit Binding / Delete
非目标: [不涉及脚本引擎]
状态: done
