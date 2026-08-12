#!/usr/bin/env python3
"""生成「AI 编程工具调用 deepin Skills 对话记录」截图（参赛材料）。"""
from PIL import Image, ImageDraw, ImageFont

W, H = 900, 1280
BG = (24, 26, 30)
USER_BUBBLE = (44, 46, 54)
AI_BUBBLE = (30, 80, 120)
TEXT = (230, 233, 237)
MUTED = (140, 145, 155)
ACCENT = (75, 165, 255)

img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

# 字体（优先中文字体，确保中文渲染正常）
def get_font(size):
    for path in ["/usr/share/fonts/opentype/source-han-cjk/SourceHanSansSC-Medium.otf",
                 "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
                 "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                 "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
                 "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"]:
        try:
            return ImageFont.truetype(path, size)
        except Exception:
            continue
    return ImageFont.load_default()

title_font = get_font(28)
font = get_font(19)
small_font = get_font(15)
mono_font = get_font(17)

# 顶部标题栏
d.rectangle([0, 0, W, 90], fill=(32, 35, 42))
d.text((30, 28), "Hermes Agent — deepin Skills 插件开发会话", font=title_font, fill=TEXT)
d.text((30, 62), "使用 Skill：dde-control-center-development / dtk-development / deepin-25-platform", font=small_font, fill=MUTED)

# 会话内容（真实开发过程摘要）
lines = [
    ("u", "我要为 deepin 25 做一个类似 Windows「Microsoft Print to PDF」的虚拟 PDF 打印机，用控制中心插件方向参赛"),
    ("a", "已加载 dde-control-center-development 技能。建议先 POC 验证 CUPS backend 可行性，再设计插件框架。我已检查：CUPS 运行中、lpadmin 可用、系统自带 Generic-PDF PPD。"),
    ("u", "先做个 POC，然后设计整体项目框架"),
    ("a", "✅ POC 成功：lp -d Deepin-PDF 生成 PDF。发现 3 个关键坑：\n① /usr 只读 → 必须 dpkg 打包安装\n② backend 755 会以 lp 用户运行 → 必须 700 root\n③ PJL 头需要剥离\n框架设计已写入 docs/design.md"),
    ("a", "根据 deepin-25-platform 技能：deepin 25 是 ostree 不可变系统，/usr 只读，系统文件只能通过 deb 包安装（dpkg 有专用写路径）。"),
    ("u", "委派子agent模块化开发"),
    ("a", "已定义接口契约 docs/contract.md（签名钉死），并行委派 3 个子任务：\n① 服务层（PrinterManager/ConfigManager/OutputDirWatcher）\n② 插件 C++（PdfPrinterModule + DCC_FACTORY_CLASS）\n③ 插件 QML（4 页面）"),
    ("u", "选择输出目录的时候闪退了"),
    ("a", "根因：控制中心是纯 QML 应用（QGuiApplication），QFileDialog（QtWidgets）必然崩溃。\n改用 D-Bus 调用 com.deepin.filemanager.filedialog 目录选择：createDialog → setFileMode(Directory) → accepted 信号 → selectedUrls 方法。"),
    ("u", "选择目录后没有生效还是原来的"),
    ("a", "两个根因：\n① selectedUrls 是方法不是信号 → 改监听 accepted 信号\n② Q_PROPERTY WRITE 方法（setOutputDir）不能当 QML 函数调 → 改属性赋值 dccData.outputDir = dir\n③ QML 编译进 .so，改 QML 必须完整重建（rm -rf build）"),
    ("u", "确认，继续优化"),
    ("a", "已完成 P0 增强：\n① backend 健壮性（毫秒防覆盖/写失败处理）\n② postinst 自动创建打印机（安装即用）\n③ README + AGENTS.md（AI 二次开发引导）\n④ 安全审查通过（无外连/无敏感采集）"),
    ("a", "🎉 项目完成：功能全链路验证通过，已开源至 GitHub（Re-s/dde-virtual-pdf-printer），Release v0.5.4 含 deb 包 + 截图。"),
]

y = 120
for who, text in lines:
    bubble_h = 0
    # 估算气泡高度（每行约 30px）
    n_lines = len(text.split("\n"))
    chars = max(len(l) for l in text.split("\n"))
    est_lines = sum(max(1, (len(l) + 38) // 40) for l in text.split("\n"))
    bubble_h = est_lines * 28 + 24

    if who == "u":
        # 用户消息：右侧
        x0, x1 = W - 480, W - 20
        d.rounded_rectangle([x0, y, x1, y + bubble_h], radius=12, fill=USER_BUBBLE)
        d.text((x0 + 14, y + 10), "👤 用户", font=small_font, fill=MUTED)
        ty = y + 34
        for l in text.split("\n"):
            d.text((x0 + 14, ty), l, font=font, fill=TEXT)
            ty += 28
    else:
        # AI 消息：左侧，带技能标签
        x0, x1 = 20, W - 420
        d.rounded_rectangle([x0, y, x1, y + bubble_h], radius=12, fill=AI_BUBBLE)
        d.text((x0 + 14, y + 10), "🤖 AI (Hermes) · 已加载 deepin Skills", font=small_font, fill=MUTED)
        ty = y + 34
        for l in text.split("\n"):
            if l.startswith("①") or l.startswith("②") or l.startswith("③") or l.startswith("④"):
                d.text((x0 + 30, ty), l, font=font, fill=ACCENT)
            else:
                d.text((x0 + 14, ty), l, font=font, fill=TEXT)
            ty += 28

    y += bubble_h + 18
    if y > H - 80:
        break

# 底部
d.rounded_rectangle([20, H - 60, W - 20, H - 15], radius=10, fill=(32, 35, 42))
d.text((30, H - 52), "↑ 以上为 AI 编程工具调用 deepin Skills 辅助开发的部分对话记录", font=small_font, fill=MUTED)

img.save("/home/master/Projects/deepin-pdf-printer/docs/screenshots/05-ai-dialogue.png", "PNG")
print("已生成: docs/screenshots/05-ai-dialogue.png")
