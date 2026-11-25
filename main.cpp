#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QScroller>
#include <QShortcut>
#include <QStack>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextStream>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace {

struct ThemeConfig {
    QString name = QStringLiteral("default");

    // Window
    int windowWidth = 484;
    int windowHeight = 467;
    QColor background = QColor("#212421");
    QColor selectionBackground = QColor("#3A423A");
    QColor selectionText = QColor("#FFFFFF");
    QColor dividerColor = QColor(255, 255, 255, 55);
    QColor scrollbarColor = QColor("#3A3F3A");

    // Header
    QString headerFontFamily = QStringLiteral("Inter");
    int headerFontSize = 14;
    QFont::Weight headerFontWeight = QFont::DemiBold;
    QColor activeTabColor = QColor("#FFFFFF");
    QColor inactiveTabColor = QColor(255, 255, 255, 128);
    QColor hoverTabColor = QColor(255, 255, 255, 204);
    QColor plusColor = QColor("#FFFFFF");
    QColor plusHoverColor = QColor("#AAFFE4");

    // Editor
    QString editorFontFamily = QStringLiteral("Inter");
    int editorFontSize = 16;
    QFont::Weight editorFontWeight = QFont::Normal;

    // Footer active tool icon color (bold, italic, bigger, smaller)
    QColor activeToolColor = QColor("#AAFFE4");

    // 4 bottom-right text/highlighter swatches (from Figma variables)
    std::array<QColor, 4> textColors = {
        QColor("#FFFFFF"), // swatch_1
        QColor("#FF8FBE"), // swatch_2
        QColor("#F4D76E"), // swatch_3
        QColor("#AAFFE4")  // bright_blue
    };
};

struct KeybindingsConfig {
    QString newTab = QStringLiteral("ff+t");
    QString newTabAlt = QStringLiteral("Ctrl+T");
    QString closeTab = QStringLiteral("Ctrl+W");
    QString reopenClosedTab = QStringLiteral("Ctrl+Shift+T");
    QString save = QStringLiteral("Ctrl+S");
    QString renameTab = QStringLiteral("ff+r");
    QString renameTabAlt = QStringLiteral("F2");
    QString nextTab = QStringLiteral("ff+k");
    QString nextTabAlt = QStringLiteral("Ctrl+Tab");
    QString prevTab = QStringLiteral("ff+j");
    QString prevTabAlt = QStringLiteral("Ctrl+Shift+Tab");
    QString zoomIn = QStringLiteral("Ctrl+=");
    QString zoomOut = QStringLiteral("Ctrl+-");
    QString zoomReset = QStringLiteral("Ctrl+0");

    QString dividerTrigger = QStringLiteral("---");

    QString leaderTrigger = QStringLiteral("jj");
    qint64 leaderTimeoutMs = 350;
    std::array<QString, 4> colorKeys = {
        QStringLiteral("j"),
        QStringLiteral("k"),
        QStringLiteral("l"),
        QStringLiteral(";")
    };
    std::array<QString, 4> formatKeys = {
        QStringLiteral("a"), // Bold
        QStringLiteral("s"), // Italic
        QStringLiteral("d"), // Font size +2px
        QStringLiteral("f")  // Font size -2px
    };

    QString newTabTrigger = QStringLiteral("ff");
    qint64 newTabTimeoutMs = 350;
    QString deleteLineTrigger = QStringLiteral("dd");
    qint64 deleteLineTimeoutMs = 350;
    QString searchTrigger = QStringLiteral("//");
    qint64 searchTimeoutMs = 350;
    QString resetFormatKey = QStringLiteral("h");
    QString highlightModeKey = QStringLiteral("u");
    int fontSizeStep = 4;
};

struct AppConfig {
    QString themeName = QStringLiteral("default");
    QString notesDir;
    KeybindingsConfig keys;
    ThemeConfig theme;
    QString configFilePath;
    QString themeFilePath;
    QString themesDirPath;
};

struct ClosedNote {
    QString title;
    QString trashFilePath;
    QJsonArray styleSpans;
    QString activeColor;
    int activeColorIndex = 0;
    bool activeBold = false;
    bool activeItalic = false;
    int activeFontSize = 16;
    int index = -1;
};

QFont::Weight parseFontWeight(const QString &w, QFont::Weight fallback) {
    QString lower = w.trimmed().toLower();
    if (lower == QLatin1String("thin")) return QFont::Thin;
    if (lower == QLatin1String("extralight")) return QFont::ExtraLight;
    if (lower == QLatin1String("light")) return QFont::Light;
    if (lower == QLatin1String("normal") || lower == QLatin1String("regular")) return QFont::Normal;
    if (lower == QLatin1String("medium")) return QFont::Medium;
    if (lower == QLatin1String("semibold") || lower == QLatin1String("demibold")) return QFont::DemiBold;
    if (lower == QLatin1String("bold")) return QFont::Bold;
    if (lower == QLatin1String("extrabold")) return QFont::ExtraBold;
    if (lower == QLatin1String("black")) return QFont::Black;
    return fallback;
}

QColor parseColor(const QString &str, const QColor &fallback) {
    QString s = str.trimmed();
    if (s.isEmpty()) {
        return fallback;
    }
    QColor c(s);
    return c.isValid() ? c : fallback;
}

double srgbChannelLuminance(double c) {
    return (c <= 0.03928) ? (c / 12.92) : std::pow((c + 0.055) / 1.055, 2.4);
}

double relativeLuminance(const QColor &c) {
    return 0.2126 * srgbChannelLuminance(c.redF())
         + 0.7152 * srgbChannelLuminance(c.greenF())
         + 0.0722 * srgbChannelLuminance(c.blueF());
}

// Picks #000000 or #FFFFFF based on whichever has the higher WCAG 2.1 contrast ratio against bg
QColor contrastTextColor(const QColor &bg) {
    double L = relativeLuminance(bg);
    double contrastWhite = (1.0 + 0.05) / (L + 0.05);
    double contrastBlack = (L + 0.05) / (0.0 + 0.05);
    return (contrastBlack >= contrastWhite) ? QColor("#000000") : QColor("#FFFFFF");
}

QString userConfigDirectory() {
#if defined(Q_OS_MACOS) || defined(Q_OS_ANDROID)
    // macOS: ~/Library/Application Support/mimenotes  |  Android: app-private data dir
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    // Linux: ~/.config/mimenotes  |  Windows: %LOCALAPPDATA%\mimenotes
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
#endif
    if (dir.isEmpty()) {
        dir = QDir::home().filePath(QStringLiteral(".config/mimenotes"));
    }
    return dir;
}

void copyResourceIfMissing(const QString &resourcePath, const QString &destPath) {
    if (QFileInfo::exists(destPath) || !QFile::exists(resourcePath)) {
        return;
    }
    QDir().mkpath(QFileInfo(destPath).absolutePath());
    if (QFile::copy(resourcePath, destPath)) {
        QFile::setPermissions(destPath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                              QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }
}

void bootstrapUserConfigIfMissing() {
    QString cfgDir = userConfigDirectory();
    if (cfgDir.isEmpty()) {
        return;
    }
    QDir dir(cfgDir);
    dir.mkpath(QStringLiteral("themes"));
    copyResourceIfMissing(QStringLiteral(":/config.json"), dir.filePath(QStringLiteral("config.json")));

    QDir resThemes(QStringLiteral(":/themes"));
    const QStringList themeFiles = resThemes.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
    for (const QString &tf : themeFiles) {
        copyResourceIfMissing(QStringLiteral(":/themes/") + tf, dir.filePath(QStringLiteral("themes/") + tf));
    }
}

QString resolveAssetSvgPath(const QString &fileName) {
    QStringList candidates;
    candidates << QDir::current().filePath("assets/" + fileName);
    candidates << QDir(QCoreApplication::applicationDirPath()).filePath("assets/" + fileName);
    candidates << QDir(userConfigDirectory()).filePath("assets/" + fileName);
    candidates << QDir::home().filePath(".config/mimenotes/assets/" + fileName);
    candidates << (QStringLiteral(":/assets/") + fileName);
    for (const QString &cand : candidates) {
        if (QFileInfo::exists(cand)) {
            return cand;
        }
    }
    return {};
}

void renderTintedSvg(QPainter &painter,
                     const QString &svgPath,
                     const QRect &targetRect,
                     const QColor &tint,
                     qreal dpr) {
    if (svgPath.isEmpty() || targetRect.isEmpty()) {
        return;
    }
    QSvgRenderer renderer(svgPath);
    if (!renderer.isValid()) {
        return;
    }
    QSize pxSize(std::max(1, qRound(targetRect.width() * dpr)),
                 std::max(1, qRound(targetRect.height() * dpr)));
    QPixmap pm(pxSize);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter pmPainter(&pm);
    pmPainter.setRenderHint(QPainter::Antialiasing, true);
    pmPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&pmPainter, QRectF(0, 0, targetRect.width(), targetRect.height()));
    pmPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    pmPainter.fillRect(QRectF(0, 0, targetRect.width(), targetRect.height()), tint);
    pmPainter.end();

    painter.drawPixmap(targetRect.topLeft(), pm);
}

QFont makeFontWithFallback(const QString &primaryFamily, int pixelSize, QFont::Weight weight, bool italic = false) {
    QFont font;
    QStringList families;
    if (!primaryFamily.trimmed().isEmpty()) {
        families.append(primaryFamily.trimmed());
    }
    families << QStringLiteral("Inter")
             << QStringLiteral("SF Pro Text")
             << QStringLiteral("Segoe UI")
             << QStringLiteral("Roboto")
             << QStringLiteral("Adwaita Sans")
             << QStringLiteral("Noto Sans")
             << QStringLiteral("Sans Serif");
    families.removeDuplicates();
    font.setFamilies(families);
    font.setPixelSize(std::clamp(pixelSize, 8, 72));
    font.setWeight(weight);
    font.setItalic(italic);
    return font;
}

QString expandTilde(const QString &path) {
    QString t = path.trimmed();
    if (t == QLatin1String("~")) {
        return QDir::homePath();
    }
    if (t.startsWith(QLatin1String("~/"))) {
        return QDir::home().filePath(t.mid(2));
    }
    return t;
}

QString defaultNotesDirectory() {
#ifdef Q_OS_ANDROID
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        return QDir(appData).filePath(QStringLiteral("Mimenotes"));
    }
#endif
    QString docsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!docsPath.isEmpty() && QDir(docsPath).exists()) {
        return QDir(docsPath).filePath(QStringLiteral("Mimenotes"));
    }
    QString homeDocs = QDir::home().filePath(QStringLiteral("Documents"));
    if (QDir(homeDocs).exists()) {
        return QDir(homeDocs).filePath(QStringLiteral("Mimenotes"));
    }
    return QDir::home().filePath(QStringLiteral("Mimenotes"));
}

QString resolveNotesDirectory(const QString &rawConfigPath) {
    QString raw = rawConfigPath.trimmed();
    if (raw.isEmpty() ||
        raw == QLatin1String("~/Documents/Mimenotes") ||
        raw == QLatin1String("~/Mimenotes") ||
        raw == QLatin1String("~/Notes")) {
        return defaultNotesDirectory();
    }
    return expandTilde(raw);
}

bool isMarkdownFile(const QString &path) {
    return path.endsWith(QLatin1String(".md"), Qt::CaseInsensitive) ||
           path.endsWith(QLatin1String(".markdown"), Qt::CaseInsensitive);
}

QString stripMarkdownForPreview(const QString &line) {
    QString s = line.trimmed();
    static const QRegularExpression kHeadingRe(QStringLiteral("^#{1,6}\\s+"));
    static const QRegularExpression kTagRe(QStringLiteral("</?(?:span|mark|small|b|strong|i|em)(?:\\s+[^>]*)?>"),
                                           QRegularExpression::CaseInsensitiveOption);
    s.remove(kHeadingRe);
    s.remove(kTagRe);
    s.replace(QStringLiteral("***"), QString());
    s.replace(QStringLiteral("**"), QString());
    s.replace(QStringLiteral("=="), QString());
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return s.trimmed();
}

QString sanitizeTitle(const QString &raw) {
    QString s = raw.trimmed();
    s.replace('/', '-');
    s.replace('\\', '-');
    s.replace('\0', "");
#ifdef Q_OS_WIN
    for (QChar ch : QStringLiteral(":*?\"<>|")) {
        s.replace(ch, '-');
    }
#endif
    while (s.startsWith('.')) {
        s.remove(0, 1);
    }
    s = s.trimmed();
    if (s.endsWith(".md", Qt::CaseInsensitive)) {
        s.chop(3);
        s = s.trimmed();
    } else if (s.endsWith(".txt", Qt::CaseInsensitive)) {
        s.chop(4);
        s = s.trimmed();
    }
#ifdef Q_OS_WIN
    while (s.endsWith('.') || s.endsWith(' ')) {
        s.chop(1);
    }
#endif
    return s;
}

QString makeTimestampPrefix() {
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss_"));
}

QString extractTimestampPrefix(const QString &filePath) {
    static const QRegularExpression kPrefixRe(QStringLiteral("^(\\d{4}-\\d{2}-\\d{2}_\\d{6}_)"));
    QString base = QFileInfo(filePath).completeBaseName();
    QRegularExpressionMatch m = kPrefixRe.match(base);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString extractDisplayTitle(const QString &filePath) {
    QString base = QFileInfo(filePath).completeBaseName();
    QString prefix = extractTimestampPrefix(filePath);
    if (!prefix.isEmpty() && base.size() > prefix.size()) {
        return base.mid(prefix.size());
    }
    return base;
}

QString extractDateLabel(const QString &filePath) {
    QString prefix = extractTimestampPrefix(filePath);
    return (prefix.size() >= 10) ? prefix.left(10) : QString();
}

ThemeConfig loadThemeFromFile(const QString &path) {
    ThemeConfig t;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return t;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return t;
    }
    QJsonObject root = doc.object();
    t.name = root["name"].toString(t.name);

    if (root["window"].isObject()) {
        QJsonObject w = root["window"].toObject();
        t.windowWidth = std::max(300, w["width"].toInt(t.windowWidth));
        t.windowHeight = std::max(240, w["height"].toInt(t.windowHeight));
        t.background = parseColor(w["background"].toString(), t.background);
        t.selectionBackground = parseColor(w["selection_background"].toString(), t.selectionBackground);
        t.selectionText = parseColor(w["selection_text"].toString(), t.selectionText);
        t.dividerColor = parseColor(w["divider"].toString(), t.dividerColor);
        t.scrollbarColor = parseColor(w["scrollbar"].toString(), t.scrollbarColor);
    }

    if (root["header"].isObject()) {
        QJsonObject h = root["header"].toObject();
        t.headerFontFamily = h["font_family"].toString(t.headerFontFamily);
        t.headerFontSize = std::clamp(h["font_size"].toInt(t.headerFontSize), 8, 48);
        t.headerFontWeight = parseFontWeight(h["font_weight"].toString(), t.headerFontWeight);
        t.activeTabColor = parseColor(h["active_tab_color"].toString(), t.activeTabColor);
        t.inactiveTabColor = parseColor(h["inactive_tab_color"].toString(), t.inactiveTabColor);
        t.hoverTabColor = parseColor(h["hover_tab_color"].toString(), t.hoverTabColor);
        t.plusColor = parseColor(h["plus_color"].toString(), t.plusColor);
        t.plusHoverColor = parseColor(h["plus_hover_color"].toString(), t.plusHoverColor);
    }

    if (root["editor"].isObject()) {
        QJsonObject e = root["editor"].toObject();
        t.editorFontFamily = e["font_family"].toString(t.editorFontFamily);
        t.editorFontSize = std::clamp(e["font_size"].toInt(t.editorFontSize), 8, 72);
        t.editorFontWeight = parseFontWeight(e["font_weight"].toString(), t.editorFontWeight);
    }

    if (root["footer"].isObject()) {
        QJsonObject f = root["footer"].toObject();
        t.activeToolColor = parseColor(f["active_tool_color"].toString(), t.activeToolColor);
    }
    if (root.contains("active_tool_color")) {
        t.activeToolColor = parseColor(root["active_tool_color"].toString(), t.activeToolColor);
    }

    if (root["text_colors"].isArray()) {
        QJsonArray arr = root["text_colors"].toArray();
        for (int i = 0; i < std::min<int>(4, arr.size()); ++i) {
            t.textColors[i] = parseColor(arr[i].toString(), t.textColors[i]);
        }
    }

    return t;
}

AppConfig loadAppConfig(const QString &explicitConfigPath, const QString &themeOverride) {
    AppConfig cfg;

    QStringList candidates;
    if (!explicitConfigPath.isEmpty()) {
        candidates << expandTilde(explicitConfigPath);
    }
    QString envCfg = qEnvironmentVariable("MIMENOTES_CONFIG");
    if (envCfg.isEmpty()) {
        envCfg = qEnvironmentVariable("NOTES_CONFIG");
    }
    if (!envCfg.isEmpty()) {
        candidates << expandTilde(envCfg);
    }
    const QString userCfgDir = userConfigDirectory();
    candidates << QDir::current().filePath("config.json");
    candidates << QDir(QCoreApplication::applicationDirPath()).filePath("config.json");
    candidates << QDir(userCfgDir).filePath("config.json");
    candidates << QDir::home().filePath(".config/mimenotes/config.json");
    candidates << QDir::home().filePath(".config/notes/config.json");

    for (const QString &cand : candidates) {
        if (QFileInfo::exists(cand)) {
            cfg.configFilePath = QFileInfo(cand).absoluteFilePath();
            break;
        }
    }

    if (cfg.configFilePath.isEmpty()) {
        bootstrapUserConfigIfMissing();
        QString bootstrapped = QDir(userCfgDir).filePath("config.json");
        if (QFileInfo::exists(bootstrapped)) {
            cfg.configFilePath = QFileInfo(bootstrapped).absoluteFilePath();
        } else if (QFile::exists(QStringLiteral(":/config.json"))) {
            cfg.configFilePath = QStringLiteral(":/config.json");
        }
    }

    int configDefaultFontSize = -1;
    QColor configActiveToolColor;
    if (!cfg.configFilePath.isEmpty()) {
        if (cfg.configFilePath.startsWith(QLatin1String(":/"))) {
            cfg.themesDirPath = QDir(userCfgDir).filePath("themes");
        } else {
            cfg.themesDirPath = QFileInfo(cfg.configFilePath).absoluteDir().filePath("themes");
        }
        QFile file(cfg.configFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                cfg.themeName = root["theme"].toString(cfg.themeName);
                if (root.contains("default_font_size")) {
                    configDefaultFontSize = std::clamp(root["default_font_size"].toInt(16), 8, 72);
                }
                if (root.contains("font_size_step")) {
                    cfg.keys.fontSizeStep = std::clamp(root["font_size_step"].toInt(4), 1, 24);
                }
                if (root.contains("active_tool_color")) {
                    configActiveToolColor = parseColor(root["active_tool_color"].toString(), QColor());
                }
                cfg.notesDir = resolveNotesDirectory(root["notes_dir"].toString());

                if (root["keybindings"].isObject()) {
                    QJsonObject kb = root["keybindings"].toObject();
                    cfg.keys.newTab = kb["new_tab"].toString(cfg.keys.newTab);
                    cfg.keys.newTabAlt = kb["new_tab_alt"].toString(cfg.keys.newTabAlt);
                    cfg.keys.closeTab = kb["close_tab"].toString(cfg.keys.closeTab);
                    cfg.keys.reopenClosedTab = kb["reopen_closed_tab"].toString(cfg.keys.reopenClosedTab);
                    cfg.keys.save = kb["save"].toString(cfg.keys.save);
                    cfg.keys.renameTab = kb["rename_tab"].toString(cfg.keys.renameTab);
                    cfg.keys.renameTabAlt = kb["rename_tab_alt"].toString(cfg.keys.renameTabAlt);
                    cfg.keys.nextTab = kb["next_tab"].toString(cfg.keys.nextTab);
                    cfg.keys.nextTabAlt = kb["next_tab_alt"].toString(cfg.keys.nextTabAlt);
                    cfg.keys.prevTab = kb["prev_tab"].toString(cfg.keys.prevTab);
                    cfg.keys.prevTabAlt = kb["prev_tab_alt"].toString(cfg.keys.prevTabAlt);
                    cfg.keys.zoomIn = kb["zoom_in"].toString(cfg.keys.zoomIn);
                    cfg.keys.zoomOut = kb["zoom_out"].toString(cfg.keys.zoomOut);
                    cfg.keys.zoomReset = kb["zoom_reset"].toString(cfg.keys.zoomReset);

                    QString divTrig = kb["divider_trigger"].toString(cfg.keys.dividerTrigger);
                    if (divTrig.size() >= 2) {
                        cfg.keys.dividerTrigger = divTrig;
                    }

                    QString leadTrig = kb["leader_trigger"].toString(
                        kb["color_mode_trigger"].toString(cfg.keys.leaderTrigger));
                    if (leadTrig.size() == 2) {
                        cfg.keys.leaderTrigger = leadTrig;
                    }
                    int leadTimeout = kb["leader_timeout_ms"].toInt(
                        kb["color_mode_timeout_ms"].toInt(static_cast<int>(cfg.keys.leaderTimeoutMs)));
                    cfg.keys.leaderTimeoutMs = std::clamp<qint64>(leadTimeout, 80, 2000);

                    if (kb["color_keys"].isArray()) {
                        QJsonArray ck = kb["color_keys"].toArray();
                        for (int i = 0; i < std::min<int>(4, ck.size()); ++i) {
                            QString k = ck[i].toString();
                            if (!k.isEmpty()) {
                                cfg.keys.colorKeys[i] = k;
                            }
                        }
                    }

                    if (kb["format_keys"].isArray()) {
                        QJsonArray fk = kb["format_keys"].toArray();
                        for (int i = 0; i < std::min<int>(4, fk.size()); ++i) {
                            QString k = fk[i].toString();
                            if (!k.isEmpty()) {
                                cfg.keys.formatKeys[i] = k;
                            }
                        }
                    }

                    QString ntTrig = kb["tab_leader_trigger"].toString(
                        kb["new_tab_trigger"].toString(cfg.keys.newTabTrigger));
                    if (ntTrig.size() == 2) {
                        cfg.keys.newTabTrigger = ntTrig;
                    }
                    int ntTimeout = kb["tab_leader_timeout_ms"].toInt(
                        kb["new_tab_timeout_ms"].toInt(static_cast<int>(cfg.keys.newTabTimeoutMs)));
                    cfg.keys.newTabTimeoutMs = std::clamp<qint64>(ntTimeout, 80, 2000);

                    QString dlTrig = kb["delete_line_trigger"].toString(cfg.keys.deleteLineTrigger);
                    if (dlTrig.size() == 2) {
                        cfg.keys.deleteLineTrigger = dlTrig;
                    }
                    int dlTimeout = kb["delete_line_timeout_ms"].toInt(static_cast<int>(cfg.keys.deleteLineTimeoutMs));
                    cfg.keys.deleteLineTimeoutMs = std::clamp<qint64>(dlTimeout, 80, 2000);

                    QString srchTrig = kb["search_trigger"].toString(cfg.keys.searchTrigger);
                    if (srchTrig.size() == 2) {
                        cfg.keys.searchTrigger = srchTrig;
                    }
                    int srchTimeout = kb["search_timeout_ms"].toInt(static_cast<int>(cfg.keys.searchTimeoutMs));
                    cfg.keys.searchTimeoutMs = std::clamp<qint64>(srchTimeout, 80, 2000);

                    QString rstKey = kb["reset_format_key"].toString(cfg.keys.resetFormatKey);
                    if (!rstKey.isEmpty()) {
                        cfg.keys.resetFormatKey = rstKey;
                    }

                    QString hlKey = kb["highlight_mode_key"].toString(cfg.keys.highlightModeKey);
                    if (!hlKey.isEmpty()) {
                        cfg.keys.highlightModeKey = hlKey;
                    }
                }
            }
        }
    } else {
        cfg.themesDirPath = QDir(QCoreApplication::applicationDirPath()).filePath("themes");
    }

    if (!themeOverride.trimmed().isEmpty()) {
        cfg.themeName = themeOverride.trimmed();
    }

    QString themeNameClean = cfg.themeName;
    if (themeNameClean.endsWith(".json", Qt::CaseInsensitive)) {
        themeNameClean.chop(5);
    }

    QStringList themeCandidates;
    if (!cfg.themesDirPath.isEmpty()) {
        themeCandidates << QDir(cfg.themesDirPath).filePath(themeNameClean + ".json");
    }
    themeCandidates << QDir::current().filePath("themes/" + themeNameClean + ".json");
    themeCandidates << QDir(QCoreApplication::applicationDirPath()).filePath("themes/" + themeNameClean + ".json");
    themeCandidates << QDir(userCfgDir).filePath("themes/" + themeNameClean + ".json");
    themeCandidates << QDir::home().filePath(".config/mimenotes/themes/" + themeNameClean + ".json");
    themeCandidates << QDir::home().filePath(".config/notes/themes/" + themeNameClean + ".json");
    themeCandidates << (QStringLiteral(":/themes/") + themeNameClean + QStringLiteral(".json"));

    for (const QString &tc : themeCandidates) {
        if (QFileInfo::exists(tc)) {
            cfg.themeFilePath = tc.startsWith(QLatin1String(":/")) ? tc : QFileInfo(tc).absoluteFilePath();
            cfg.theme = loadThemeFromFile(cfg.themeFilePath);
            break;
        }
    }

    if (configDefaultFontSize > 0) {
        cfg.theme.editorFontSize = configDefaultFontSize;
    }
    if (configActiveToolColor.isValid()) {
        cfg.theme.activeToolColor = configActiveToolColor;
    }

    return cfg;
}

// Plain-text editor supporting:
// - per-character colors from the 4 theme colors ('jj' + 'j'/'k'/'l'/';' or bottom-right dots)
// - per-character bold, italic, and +/-2px font size ('ff' + 'a'/'s'/'d'/'f' or bottom-left icons)
// - automatic horizontal dividers when the user types the divider trigger (default "---")
class NoteEditor : public QTextEdit {
public:
    explicit NoteEditor(const ThemeConfig &theme,
                        const KeybindingsConfig &keys,
                        QWidget *parent = nullptr)
        : QTextEdit(parent),
          theme_(theme),
          keys_(keys),
          activeColor_(theme.textColors[0]),
          activeColorIndex_(0),
          activeFontSize_(theme.editorFontSize) {
        setFrameStyle(QFrame::NoFrame);
        setAcceptRichText(false);
        document()->setDocumentMargin(0);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#ifdef Q_OS_ANDROID
        QScroller::grabGesture(viewport(), QScroller::TouchGesture);
#endif

        applyThemeStyle();
        applyTypingFormatToCursor();

        connect(this, &QTextEdit::cursorPositionChanged, this, [this]() {
            if (suppressCursorSync_) {
                return;
            }
            QTextBlock blk = textCursor().block();
            if (!blk.isValid() || isDividerBlock(blk)) {
                return;
            }
            int blkPx = pixelSizeForTier(blockSizeTier(blk));
            if (activeFontSize_ != blkPx) {
                activeFontSize_ = blkPx;
                applyTypingFormatToCursor();
                if (onToolbarSyncNeeded) {
                    onToolbarSyncNeeded();
                }
            }
        });
    }

    static constexpr int kSmallBlockProperty = QTextFormat::UserProperty + 1;

    std::function<void(bool)> onLeaderModeChanged;
    std::function<void()> onFormattingChanged;
    std::function<void()> onToolbarSyncNeeded;
    std::function<void()> onNewTabTriggered;
    std::function<void()> onPrevTabTriggered;
    std::function<void()> onNextTabTriggered;
    std::function<void()> onRenameTabTriggered;
    std::function<void()> onSearchTriggered;

    QColor activeTypingColor() const { return activeColor_; }
    int activeTypingColorIndex() const { return activeColorIndex_; }
    bool isBoldActive() const { return activeBold_; }
    bool isItalicActive() const { return activeItalic_; }
    bool isHighlightMode() const { return highlightMode_; }
    int activeFontSize() const { return activeFontSize_; }

    int pixelSizeForTier(int tier) const {
        int clamped = std::clamp(tier, -1, 3);
        return std::clamp(theme_.editorFontSize + clamped * keys_.fontSizeStep, 10, 72);
    }

    int blockSizeTier(const QTextBlock &block) const {
        if (!block.isValid()) {
            return 0;
        }
        QTextBlockFormat bfmt = block.blockFormat();
        int h = bfmt.headingLevel();
        if (h == 1) return 3; // #
        if (h == 2) return 2; // ##
        if (h == 3) return 1; // ###
        if (h > 3) return 1;
        if (bfmt.boolProperty(kSmallBlockProperty)) {
            return -1; // <small>
        }
        return 0;
    }

    void updateConfig(const ThemeConfig &newTheme, const KeybindingsConfig &newKeys) {
        std::array<QColor, 4> oldPalette = theme_.textColors;
        theme_ = newTheme;
        keys_ = newKeys;
        applyThemeStyle();

        suppressCursorSync_ = true;

        // Remap existing styled text spans and heading sizes
        QTextBlock block = document()->begin();
        QTextCursor cursor(document());
        while (block.isValid()) {
            if (!isDividerBlock(block)) {
                int tier = blockSizeTier(block);
                int tierPx = pixelSizeForTier(tier);
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    QTextFragment frag = it.fragment();
                    if (!frag.isValid()) continue;
                    QTextCharFormat curFmt = frag.charFormat();
                    QFont curFont = curFmt.font();
                    bool b = curFont.weight() >= QFont::Bold;
                    bool itl = curFont.italic();
                    int px = (tier != 0) ? tierPx
                                         : (curFont.pixelSize() > 0 ? curFont.pixelSize() : theme_.editorFontSize);

                    cursor.setPosition(frag.position());
                    cursor.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                    QTextCharFormat fmt;
                    if (curFmt.background().style() != Qt::NoBrush &&
                        curFmt.background().color().isValid() &&
                        curFmt.background().color().alpha() > 0) {
                        QColor bgCol = curFmt.background().color();
                        int bgSlot = matchColorSlot(bgCol, oldPalette);
                        QColor mappedBg = (bgSlot >= 0) ? theme_.textColors[bgSlot] : bgCol;
                        fmt.setBackground(mappedBg);
                        fmt.setForeground(contrastTextColor(mappedBg));
                    } else {
                        QColor fragCol = curFmt.foreground().color();
                        int slot = matchColorSlot(fragCol, oldPalette);
                        fmt.setBackground(Qt::NoBrush);
                        fmt.setForeground(slot >= 0 ? theme_.textColors[slot] : fragCol);
                    }
                    fmt.setFont(makeFontWithFallback(
                        theme_.editorFontFamily,
                        px,
                        b ? QFont::Bold : theme_.editorFontWeight,
                        itl));
                    cursor.mergeCharFormat(fmt);
                }
            }
            block = block.next();
        }

        if (activeColorIndex_ >= 0 && activeColorIndex_ < 4) {
            activeColor_ = theme_.textColors[activeColorIndex_];
        }
        activeFontSize_ = pixelSizeForTier(blockSizeTier(textCursor().block()));
        formatExistingDividers();
        applyTypingFormatToCursor();
        suppressCursorSync_ = false;
        viewport()->update();
    }

    void setActiveTypingColorSlot(int slotIndex, bool applyToSelection = true) {
        int clamped = std::clamp(slotIndex, 0, 3);
        activeColorIndex_ = clamped;
        setActiveTypingColor(theme_.textColors[clamped], applyToSelection);
    }

    void setActiveTypingColor(const QColor &color, bool applyToSelection = true) {
        activeColor_ = color;
        int slot = matchColorSlot(color, theme_.textColors);
        if (slot >= 0) {
            activeColorIndex_ = slot;
        }
        QTextCursor cursor = textCursor();
        if (applyToSelection && cursor.hasSelection()) {
            QTextCharFormat fmt;
            if (highlightMode_) {
                fmt.setBackground(activeColor_);
                fmt.setForeground(contrastTextColor(activeColor_));
            } else {
                fmt.setBackground(Qt::NoBrush);
                fmt.setForeground(activeColor_);
            }
            cursor.mergeCharFormat(fmt);
            formatExistingDividers();
        }
        applyTypingFormatToCursor();
        if (onFormattingChanged) {
            onFormattingChanged();
        }
    }

    void setFormattingState(bool bold, bool italic, int fontSize, bool highlight = false) {
        activeBold_ = bold;
        activeItalic_ = italic;
        activeFontSize_ = std::clamp(fontSize, 8, 72);
        highlightMode_ = highlight;
        applyTypingFormatToCursor();
    }

    void toggleHighlightMode() {
        highlightMode_ = !highlightMode_;
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection()) {
            QTextCharFormat fmt;
            if (highlightMode_) {
                fmt.setBackground(activeColor_);
                fmt.setForeground(contrastTextColor(activeColor_));
            } else {
                fmt.setBackground(Qt::NoBrush);
                fmt.setForeground(activeColor_);
            }
            cursor.mergeCharFormat(fmt);
            formatExistingDividers();
        }
        applyTypingFormatToCursor();
        if (onFormattingChanged) {
            onFormattingChanged();
        }
    }

    void toggleBold() {
        activeBold_ = !activeBold_;
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection()) {
            applySpanFontOverride(cursor, [this](bool &, bool &b, int &) {
                b = activeBold_;
            });
        }
        applyTypingFormatToCursor();
        if (onFormattingChanged) {
            onFormattingChanged();
        }
    }

    void toggleItalic() {
        activeItalic_ = !activeItalic_;
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection()) {
            applySpanFontOverride(cursor, [this](bool &itl, bool &, int &) {
                itl = activeItalic_;
            });
        }
        applyTypingFormatToCursor();
        if (onFormattingChanged) {
            onFormattingChanged();
        }
    }

    void stepHeadingTier(int deltaTier) {
        if (deltaTier == 0) {
            return;
        }
        QTextCursor cursor = textCursor();
        int selStart = cursor.hasSelection() ? cursor.selectionStart() : cursor.position();
        int selEnd = cursor.hasSelection() ? cursor.selectionEnd() : cursor.position();

        QTextBlock startBlk = document()->findBlock(selStart);
        QTextBlock endBlk = document()->findBlock(selEnd);
        if (cursor.hasSelection() && endBlk.isValid() && selEnd == endBlk.position() && endBlk != startBlk) {
            endBlk = endBlk.previous();
        }

        suppressCursorSync_ = true;
        QTextCursor editCursor(document());
        editCursor.beginEditBlock();
        int lastTier = blockSizeTier(startBlk);
        QTextBlock blk = startBlk;
        while (blk.isValid()) {
            if (!isDividerBlock(blk)) {
                int newTier = std::clamp(blockSizeTier(blk) + deltaTier, -1, 3);
                applyTierToBlock(blk, newTier);
                lastTier = newTier;
            }
            if (blk == endBlk) {
                break;
            }
            blk = blk.next();
        }
        editCursor.endEditBlock();
        activeFontSize_ = pixelSizeForTier(lastTier);
        applyTypingFormatToCursor();
        suppressCursorSync_ = false;
        if (onFormattingChanged) {
            onFormattingChanged();
        }
    }

    void resetHeadingTier() {
        QTextCursor cursor = textCursor();
        int selStart = cursor.hasSelection() ? cursor.selectionStart() : cursor.position();
        int selEnd = cursor.hasSelection() ? cursor.selectionEnd() : cursor.position();

        QTextBlock startBlk = document()->findBlock(selStart);
        QTextBlock endBlk = document()->findBlock(selEnd);
        if (cursor.hasSelection() && endBlk.isValid() && selEnd == endBlk.position() && endBlk != startBlk) {
            endBlk = endBlk.previous();
        }

        suppressCursorSync_ = true;
        QTextCursor editCursor(document());
        editCursor.beginEditBlock();
        QTextBlock blk = startBlk;
        while (blk.isValid()) {
            if (!isDividerBlock(blk)) {
                applyTierToBlock(blk, 0);
            }
            if (blk == endBlk) {
                break;
            }
            blk = blk.next();
        }
        editCursor.endEditBlock();
        activeFontSize_ = pixelSizeForTier(0);
        applyTypingFormatToCursor();
        suppressCursorSync_ = false;
        if (onFormattingChanged) {
            onFormattingChanged();
        }
    }

    void adjustTypingFontSize(int deltaPx) {
        if (deltaPx > 0) {
            stepHeadingTier(1);
        } else if (deltaPx < 0) {
            stepHeadingTier(-1);
        } else {
            resetHeadingTier();
        }
    }

    void resetFormatting() {
        activeColorIndex_ = 0;
        activeColor_ = theme_.textColors[0];
        activeBold_ = false;
        activeItalic_ = false;
        highlightMode_ = false;
        activeFontSize_ = theme_.editorFontSize;

        suppressCursorSync_ = true;
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection()) {
            int selStart = cursor.selectionStart();
            int selEnd = cursor.selectionEnd();
            QTextBlock startBlk = document()->findBlock(selStart);
            QTextBlock endBlk = document()->findBlock(selEnd);
            if (endBlk.isValid() && selEnd == endBlk.position() && endBlk != startBlk) {
                endBlk = endBlk.previous();
            }
            QTextCursor editCursor(document());
            editCursor.beginEditBlock();
            QTextBlock blk = startBlk;
            while (blk.isValid()) {
                if (!isDividerBlock(blk)) {
                    applyTierToBlock(blk, 0);
                }
                if (blk == endBlk) break;
                blk = blk.next();
            }
            QTextCharFormat fmt = currentTypingCharFormat();
            cursor.mergeCharFormat(fmt);
            editCursor.endEditBlock();
            formatExistingDividers();
        } else if (cursor.block().isValid() && !isDividerBlock(cursor.block()) && blockSizeTier(cursor.block()) != 0) {
            QTextCursor editCursor(document());
            editCursor.beginEditBlock();
            applyTierToBlock(cursor.block(), 0);
            editCursor.endEditBlock();
        }
        applyTypingFormatToCursor();
        suppressCursorSync_ = false;
        if (onFormattingChanged) {
            onFormattingChanged();
        }
    }

    void formatExistingDividers() {
        QTextBlock block = document()->begin();
        while (block.isValid()) {
            if (isDividerBlock(block)) {
                QTextCursor c(block);
                c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                QTextCharFormat fmt;
                fmt.setForeground(Qt::transparent);
                c.mergeCharFormat(fmt);
            }
            block = block.next();
        }
        viewport()->update();
    }

    QJsonArray exportStyleSpans() const {
        QJsonArray spans;
        QTextBlock block = document()->begin();
        while (block.isValid()) {
            if (isDividerBlock(block)) {
                block = block.next();
                continue;
            }
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                QTextFragment frag = it.fragment();
                if (!frag.isValid()) {
                    continue;
                }
                QTextCharFormat fmt = frag.charFormat();
                QColor col = fmt.foreground().color();
                if (!col.isValid() || col.alpha() == 0) {
                    col = theme_.textColors[0];
                }
                QFont f = fmt.font();
                bool bold = f.weight() >= QFont::Bold;
                bool italic = f.italic();
                int size = f.pixelSize() > 0 ? f.pixelSize() : theme_.editorFontSize;
                int slot = matchColorSlot(col, theme_.textColors);

                QJsonObject obj;
                obj["pos"] = frag.position();
                obj["len"] = frag.length();
                obj["color"] = col.name(QColor::HexRgb).toUpper();
                if (slot >= 0) {
                    obj["slot"] = slot;
                }
                if (fmt.background().style() != Qt::NoBrush) {
                    QColor bgCol = fmt.background().color();
                    if (bgCol.isValid() && bgCol.alpha() > 0) {
                        obj["bg"] = bgCol.name(QColor::HexRgb).toUpper();
                        int bgSlot = matchColorSlot(bgCol, theme_.textColors);
                        if (bgSlot >= 0) {
                            obj["bg_slot"] = bgSlot;
                        }
                    }
                }
                obj["bold"] = bold;
                obj["italic"] = italic;
                obj["size"] = size;
                spans.append(obj);
            }
            block = block.next();
        }
        return spans;
    }

    void importStyleSpans(const QJsonArray &spans) {
        QTextCursor cursor(document());
        int docLen = document()->characterCount();
        for (const QJsonValue &val : spans) {
            QJsonObject obj = val.toObject();
            int pos = obj["pos"].toInt();
            int len = obj["len"].toInt();
            int slot = obj.contains("slot") ? obj["slot"].toInt(-1) : -1;
            QColor col = (slot >= 0 && slot < 4)
                             ? theme_.textColors[slot]
                             : QColor(obj["color"].toString());
            if (!col.isValid() || pos < 0 || len <= 0 || pos >= docLen) {
                continue;
            }
            int end = std::min(pos + len, std::max(0, docLen - 1));
            if (end <= pos) {
                continue;
            }
            bool bold = obj["bold"].toBool(false);
            bool italic = obj["italic"].toBool(false);
            int size = std::clamp(obj["size"].toInt(theme_.editorFontSize), 8, 72);

            cursor.setPosition(pos);
            cursor.setPosition(end, QTextCursor::KeepAnchor);
            QTextCharFormat fmt;
            if (obj.contains("bg") || obj.contains("bg_slot")) {
                int bgSlot = obj.contains("bg_slot") ? obj["bg_slot"].toInt(-1) : -1;
                QColor bgCol = (bgSlot >= 0 && bgSlot < 4)
                                   ? theme_.textColors[bgSlot]
                                   : QColor(obj["bg"].toString());
                if (bgCol.isValid()) {
                    fmt.setBackground(bgCol);
                    fmt.setForeground(contrastTextColor(bgCol));
                } else {
                    fmt.setForeground(col);
                }
            } else {
                fmt.setForeground(col);
            }
            fmt.setFont(makeFontWithFallback(
                theme_.editorFontFamily,
                size,
                bold ? QFont::Bold : theme_.editorFontWeight,
                italic));
            cursor.mergeCharFormat(fmt);
        }
        formatExistingDividers();
        applyTypingFormatToCursor();
    }

    QString exportMarkdown() const {
        struct MdRun {
            QString text;
            bool bold = false;
            bool italic = false;
            QString fgHex; // empty => default theme text color
            QString bgHex; // empty => no highlight
        };

        auto escapeForMd = [](const QString &raw) -> QString {
            QString out;
            out.reserve(raw.size());
            for (int i = 0; i < raw.size(); ++i) {
                QChar ch = raw[i];
                if (ch == QLatin1Char('<') && i + 1 < raw.size() &&
                    (raw[i + 1].isLetter() || raw[i + 1] == QLatin1Char('/'))) {
                    out += QStringLiteral("&lt;");
                } else {
                    out += ch;
                }
            }
            return out;
        };

        auto formatStyledRuns = [&](const QList<MdRun> &runs, bool isHighlighted) -> QString {
            QString out;
            bool prevEndedWithStar = false;
            for (const MdRun &r : runs) {
                if (r.text.isEmpty()) {
                    continue;
                }
                if (!r.bold && !r.italic) {
                    QString esc = escapeForMd(r.text);
                    out += esc;
                    prevEndedWithStar = esc.endsWith(QLatin1Char('*'));
                    continue;
                }

                // Keep leading/trailing whitespace outside Markdown * delimiters
                int firstNonSpace = 0;
                while (firstNonSpace < r.text.size() && r.text[firstNonSpace].isSpace()) {
                    ++firstNonSpace;
                }
                int lastNonSpace = r.text.size() - 1;
                while (lastNonSpace >= firstNonSpace && r.text[lastNonSpace].isSpace()) {
                    --lastNonSpace;
                }

                if (firstNonSpace > lastNonSpace) {
                    // Whitespace-only run
                    QString esc = escapeForMd(r.text);
                    out += esc;
                    prevEndedWithStar = false;
                    Q_UNUSED(isHighlighted);
                    continue;
                }

                QString lead = escapeForMd(r.text.left(firstNonSpace));
                QString core = escapeForMd(r.text.mid(firstNonSpace, lastNonSpace - firstNonSpace + 1));
                QString trail = escapeForMd(r.text.mid(lastNonSpace + 1));

                bool useHtmlTags = prevEndedWithStar && lead.isEmpty();
                out += lead;
                if (useHtmlTags) {
                    if (r.bold && r.italic) {
                        out += QStringLiteral("<b><i>") + core + QStringLiteral("</i></b>");
                    } else if (r.bold) {
                        out += QStringLiteral("<b>") + core + QStringLiteral("</b>");
                    } else {
                        out += QStringLiteral("<i>") + core + QStringLiteral("</i>");
                    }
                    prevEndedWithStar = false;
                } else {
                    QString marker = (r.bold && r.italic)
                                         ? QStringLiteral("***")
                                         : (r.bold ? QStringLiteral("**") : QStringLiteral("*"));
                    out += marker + core + marker;
                    prevEndedWithStar = trail.isEmpty();
                }
                out += trail;
            }
            return out;
        };

        QStringList lines;
        QTextBlock block = document()->begin();
        const QRgb defaultRgb = theme_.textColors[0].rgb();

        while (block.isValid()) {
            if (isDividerBlock(block)) {
                lines.append(QStringLiteral("---"));
                block = block.next();
                continue;
            }

            int tier = blockSizeTier(block);
            QList<MdRun> runs;
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                QTextFragment frag = it.fragment();
                if (!frag.isValid()) continue;
                QString txt = frag.text();
                if (txt.isEmpty()) continue;

                QTextCharFormat fmt = frag.charFormat();
                QFont f = fmt.font();
                bool bold = f.weight() >= QFont::Bold;
                bool italic = f.italic();

                QString bgHex;
                if (fmt.background().style() != Qt::NoBrush) {
                    QColor bgCol = fmt.background().color();
                    if (bgCol.isValid() && bgCol.alpha() > 0) {
                        bgHex = bgCol.name(QColor::HexRgb).toUpper();
                    }
                }

                QString fgHex;
                if (bgHex.isEmpty()) {
                    QColor fgCol = fmt.foreground().color();
                    if (fgCol.isValid() && fgCol.alpha() > 0 && fgCol.rgb() != defaultRgb) {
                        fgHex = fgCol.name(QColor::HexRgb).toUpper();
                    }
                }

                if (!runs.isEmpty() &&
                    runs.last().bold == bold &&
                    runs.last().italic == italic &&
                    runs.last().fgHex == fgHex &&
                    runs.last().bgHex == bgHex) {
                    runs.last().text += txt;
                } else {
                    runs.append(MdRun{txt, bold, italic, fgHex, bgHex});
                }
            }

            // Group contiguous runs sharing the same (fgHex, bgHex) for clean outer <span>/<mark> tags
            QString lineMd;
            int idx = 0;
            while (idx < runs.size()) {
                const QString fgHex = runs[idx].fgHex;
                const QString bgHex = runs[idx].bgHex;
                QList<MdRun> group;
                while (idx < runs.size() && runs[idx].fgHex == fgHex && runs[idx].bgHex == bgHex) {
                    group.append(runs[idx]);
                    ++idx;
                }

                QString inner = formatStyledRuns(group, !bgHex.isEmpty());
                if (!bgHex.isEmpty()) {
                    QString contrastHex = contrastTextColor(QColor(bgHex)).name(QColor::HexRgb).toUpper();
                    lineMd += QStringLiteral("<mark style=\"background:%1;color:%2\">%3</mark>")
                                  .arg(bgHex, contrastHex, inner);
                } else if (!fgHex.isEmpty()) {
                    lineMd += QStringLiteral("<span style=\"color:%1\">%2</span>").arg(fgHex, inner);
                } else {
                    lineMd += inner;
                }
            }

            if (tier == 3) {
                lines.append(QStringLiteral("# ") + lineMd);
            } else if (tier == 2) {
                lines.append(QStringLiteral("## ") + lineMd);
            } else if (tier == 1) {
                lines.append(QStringLiteral("### ") + lineMd);
            } else if (tier == -1) {
                lines.append(lineMd.isEmpty() ? QString() : (QStringLiteral("<small>") + lineMd + QStringLiteral("</small>")));
            } else {
                lines.append(lineMd);
            }

            block = block.next();
        }

        return lines.join(QLatin1Char('\n'));
    }

    void importMarkdown(const QString &markdown) {
        suppressCursorSync_ = true;
        document()->clear();

        QString normalized = markdown;
        normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        QStringList lines = normalized.split(QLatin1Char('\n'));

        static const QRegularExpression kHrRe(QStringLiteral("^\\s*(?:(?:-\\s*){3,}|(?:\\*\\s*){3,}|(?:_\\s*){3,})\\s*$"));
        static const QRegularExpression kHeadingRe(QStringLiteral("^(#{1,6})\\s+(.*)$"));

        QTextCursor cursor(document());
        cursor.beginEditBlock();

        for (int i = 0; i < lines.size(); ++i) {
            if (i > 0) {
                cursor.insertBlock();
            }
            QString line = lines[i];

            if (kHrRe.match(line).hasMatch()) {
                QTextBlockFormat bfmt;
                bfmt.setHeadingLevel(0);
                bfmt.setProperty(kSmallBlockProperty, false);
                cursor.setBlockFormat(bfmt);

                QTextCharFormat divFmt;
                divFmt.setForeground(Qt::transparent);
                divFmt.setFont(makeFontWithFallback(
                    theme_.editorFontFamily,
                    theme_.editorFontSize,
                    theme_.editorFontWeight,
                    false));
                cursor.insertText(QStringLiteral("---"), divFmt);
                continue;
            }

            int tier = 0;
            QRegularExpressionMatch hm = kHeadingRe.match(line);
            if (hm.hasMatch()) {
                int hashes = hm.captured(1).size();
                if (hashes == 1) tier = 3;
                else if (hashes == 2) tier = 2;
                else tier = 1;
                line = hm.captured(2);
            } else {
                QString trimmed = line.trimmed();
                if (trimmed.startsWith(QStringLiteral("<small>"), Qt::CaseInsensitive) &&
                    trimmed.endsWith(QStringLiteral("</small>"), Qt::CaseInsensitive)) {
                    auto [closeStart, closeEnd] = findClosingHtmlTag(trimmed, 7, QStringLiteral("small"));
                    if (closeEnd == trimmed.size() && closeStart >= 7) {
                        tier = -1;
                        line = trimmed.mid(7, closeStart - 7);
                    }
                }
            }

            QTextBlockFormat bfmt;
            bfmt.setHeadingLevel(tier == 3 ? 1 : (tier == 2 ? 2 : (tier == 1 ? 3 : 0)));
            bfmt.setProperty(kSmallBlockProperty, tier == -1);
            cursor.setBlockFormat(bfmt);

            int px = pixelSizeForTier(tier);
            QTextCharFormat defaultBlockFmt;
            defaultBlockFmt.setForeground(theme_.textColors[0]);
            defaultBlockFmt.setBackground(Qt::NoBrush);
            defaultBlockFmt.setFont(makeFontWithFallback(
                theme_.editorFontFamily,
                px,
                theme_.editorFontWeight,
                false));
            cursor.setBlockCharFormat(defaultBlockFmt);

            InlineMdState baseState;
            baseState.pixelSize = px;
            parseMarkdownInline(cursor, line, baseState);
        }

        cursor.endEditBlock();

        QTextCursor startCursor(document());
        startCursor.movePosition(QTextCursor::Start);
        setTextCursor(startCursor);
        activeFontSize_ = pixelSizeForTier(blockSizeTier(startCursor.block()));
        formatExistingDividers();
        applyTypingFormatToCursor();
        suppressCursorSync_ = false;
    }

protected:
    void focusOutEvent(QFocusEvent *event) override {
        setLeaderMode(false);
        tabLeaderMode_ = false;
        lastLeaderTriggerMs_ = 0;
        lastNewTabTriggerMs_ = 0;
        lastDeleteLineTriggerMs_ = 0;
        lastSearchTriggerMs_ = 0;
        QTextEdit::focusOutEvent(event);
    }

    void paintEvent(QPaintEvent *event) override {
        QTextEdit::paintEvent(event);

        // Draw crisp full-width horizontal divider lines for any divider block
        QPainter p(viewport());
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(theme_.dividerColor, 1.0));

        QTextBlock block = document()->begin();
        int vpHeight = viewport()->height();
        int vpWidth = viewport()->width();

        while (block.isValid()) {
            if (block.isVisible() && isDividerBlock(block)) {
                QTextCursor bc(block);
                QRect r = cursorRect(bc);
                if (r.bottom() >= 0 && r.top() <= vpHeight) {
                    int y = r.center().y();
                    p.drawLine(0, y, vpWidth, y);
                }
            }
            block = block.next();
        }
    }

    void keyPressEvent(QKeyEvent *event) override {
        int key = event->key();
        if (key == Qt::Key_Shift || key == Qt::Key_Control ||
            key == Qt::Key_Alt || key == Qt::Key_Meta) {
            QTextEdit::keyPressEvent(event);
            return;
        }

        // 1A. Handle active tab leader mode (entered via quick tab_leader_trigger chord, default "ff"):
        //     - ff+t -> new_tab, ff+j -> prev_tab, ff+k -> next_tab, ff+r -> rename_tab
        if (tabLeaderMode_) {
            tabLeaderMode_ = false;
            lastNewTabTriggerMs_ = 0;
            lastLeaderTriggerMs_ = 0;

            QString txt = event->text();
            QString newKey = extractTabChordKey(keys_.newTab, keys_.newTabAlt, QStringLiteral("t"));
            QString prevKey = extractTabChordKey(keys_.prevTab, keys_.prevTabAlt, QStringLiteral("j"));
            QString nextKey = extractTabChordKey(keys_.nextTab, keys_.nextTabAlt, QStringLiteral("k"));
            QString renKey = extractTabChordKey(keys_.renameTab, keys_.renameTabAlt, QStringLiteral("r"));

            if (!txt.isEmpty() && txt.compare(newKey, Qt::CaseInsensitive) == 0) {
                if (onNewTabTriggered) onNewTabTriggered();
                event->accept();
                return;
            }
            if (!txt.isEmpty() && txt.compare(prevKey, Qt::CaseInsensitive) == 0) {
                if (onPrevTabTriggered) onPrevTabTriggered();
                event->accept();
                return;
            }
            if (!txt.isEmpty() && txt.compare(nextKey, Qt::CaseInsensitive) == 0) {
                if (onNextTabTriggered) onNextTabTriggered();
                event->accept();
                return;
            }
            if (!txt.isEmpty() && txt.compare(renKey, Qt::CaseInsensitive) == 0) {
                if (onRenameTabTriggered) onRenameTabTriggered();
                event->accept();
                return;
            }
            if (key == Qt::Key_Escape) {
                event->accept();
                return;
            }
        }

        // 1B. Handle active leader mode (entered via quick leader_trigger chord, default "jj"):
        //     - j, k, l, ; -> Colors 1..4
        //     - a -> Bold, s -> Italic, d -> Font size +step, f -> Font size -step, h -> Reset
        if (leaderMode_) {
            QString txt = event->text();
            int colorIndex = -1;
            for (int i = 0; i < 4; ++i) {
                if (!txt.isEmpty() && txt.compare(keys_.colorKeys[i], Qt::CaseInsensitive) == 0) {
                    colorIndex = i;
                    break;
                }
            }

            int fmtAction = -1;
            for (int i = 0; i < 4; ++i) {
                if (!txt.isEmpty() && txt.compare(keys_.formatKeys[i], Qt::CaseInsensitive) == 0) {
                    fmtAction = i;
                    break;
                }
            }

            setLeaderMode(false);
            lastLeaderTriggerMs_ = 0;
            lastNewTabTriggerMs_ = 0;

            if (!txt.isEmpty() && txt.compare(keys_.highlightModeKey, Qt::CaseInsensitive) == 0) {
                toggleHighlightMode();
                event->accept();
                return;
            }
            if (!txt.isEmpty() && txt.compare(keys_.resetFormatKey, Qt::CaseInsensitive) == 0) {
                resetFormatting();
                event->accept();
                return;
            }
            if (colorIndex >= 0) {
                setActiveTypingColorSlot(colorIndex, true);
                event->accept();
                return;
            }
            if (fmtAction == 0) {
                toggleBold();
                event->accept();
                return;
            } else if (fmtAction == 1) {
                toggleItalic();
                event->accept();
                return;
            } else if (fmtAction == 2) {
                adjustTypingFontSize(keys_.fontSizeStep);
                event->accept();
                return;
            } else if (fmtAction == 3) {
                adjustTypingFontSize(-keys_.fontSizeStep);
                event->accept();
                return;
            }

            if (key == Qt::Key_Escape) {
                event->accept();
                return;
            }
        }

        bool noModifiers = !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        qint64 now = QDateTime::currentMSecsSinceEpoch();

        // 2A. Detect quick 2-character leader_trigger chord (default "jj")
        const QString &leadTrig = keys_.leaderTrigger;
        if (noModifiers && leadTrig.size() == 2 && !event->text().isEmpty()) {
            QString firstChar = leadTrig.left(1);
            QString secondChar = leadTrig.mid(1, 1);

            if (event->text() == secondChar &&
                lastLeaderTriggerMs_ > 0 &&
                (now - lastLeaderTriggerMs_) <= keys_.leaderTimeoutMs) {
                QTextCursor c = textCursor();
                int posInBlk = c.positionInBlock();
                if (!c.hasSelection() && posInBlk > 0 &&
                    c.block().text().mid(posInBlk - 1, 1) == firstChar) {
                    c.deletePreviousChar();
                    setTextCursor(c);
                    lastLeaderTriggerMs_ = 0;
                    lastNewTabTriggerMs_ = 0;
                    setLeaderMode(true);
                    event->accept();
                    return;
                }
            }
            lastLeaderTriggerMs_ = (event->text() == firstChar) ? now : 0;
        } else {
            lastLeaderTriggerMs_ = 0;
        }

        // 2B. Detect quick 2-character tab_leader_trigger chord (default "ff")
        //     Enters tab leader mode for ff+t (new_tab), ff+j (prev_tab), ff+k (next_tab), ff+r (rename_tab).
        const QString &ntTrig = keys_.newTabTrigger;
        if (noModifiers && ntTrig.size() == 2 && !event->text().isEmpty()) {
            QString firstChar = ntTrig.left(1);
            QString secondChar = ntTrig.mid(1, 1);

            if (event->text() == secondChar &&
                lastNewTabTriggerMs_ > 0 &&
                (now - lastNewTabTriggerMs_) <= keys_.newTabTimeoutMs) {
                QTextCursor c = textCursor();
                int posInBlk = c.positionInBlock();
                if (!c.hasSelection() && posInBlk > 0 &&
                    c.block().text().mid(posInBlk - 1, 1) == firstChar) {
                    c.deletePreviousChar();
                    setTextCursor(c);
                    lastNewTabTriggerMs_ = 0;
                    lastLeaderTriggerMs_ = 0;
                    setLeaderMode(false);
                    tabLeaderMode_ = true;
                    event->accept();
                    return;
                }
            }
            lastNewTabTriggerMs_ = (event->text() == firstChar) ? now : 0;
        } else {
            lastNewTabTriggerMs_ = 0;
        }

        // 2C. Detect quick 2-character delete_line_trigger chord (default "dd") -> deletes current line
        const QString &dlTrig = keys_.deleteLineTrigger;
        if (noModifiers && dlTrig.size() == 2 && !event->text().isEmpty()) {
            QString firstChar = dlTrig.left(1);
            QString secondChar = dlTrig.mid(1, 1);

            if (event->text() == secondChar &&
                lastDeleteLineTriggerMs_ > 0 &&
                (now - lastDeleteLineTriggerMs_) <= keys_.deleteLineTimeoutMs) {
                QTextCursor c = textCursor();
                int posInBlk = c.positionInBlock();
                if (!c.hasSelection() && posInBlk > 0 &&
                    c.block().text().mid(posInBlk - 1, 1) == firstChar) {
                    lastDeleteLineTriggerMs_ = 0;
                    lastLeaderTriggerMs_ = 0;
                    lastNewTabTriggerMs_ = 0;
                    setLeaderMode(false);
                    tabLeaderMode_ = false;
                    removeDividerBlock(c.block());
                    event->accept();
                    return;
                }
            }
            lastDeleteLineTriggerMs_ = (event->text() == firstChar) ? now : 0;
        } else {
            lastDeleteLineTriggerMs_ = 0;
        }

        // 2D. Detect quick 2-character search_trigger chord (default "//") -> opens searchable notes overlay
        const QString &srchTrig = keys_.searchTrigger;
        if (noModifiers && srchTrig.size() == 2 && !event->text().isEmpty()) {
            QString firstChar = srchTrig.left(1);
            QString secondChar = srchTrig.mid(1, 1);

            if (event->text() == secondChar &&
                lastSearchTriggerMs_ > 0 &&
                (now - lastSearchTriggerMs_) <= keys_.searchTimeoutMs) {
                QTextCursor c = textCursor();
                int posInBlk = c.positionInBlock();
                if (!c.hasSelection() && posInBlk > 0 &&
                    c.block().text().mid(posInBlk - 1, 1) == firstChar) {
                    c.deletePreviousChar();
                    setTextCursor(c);
                    lastSearchTriggerMs_ = 0;
                    lastDeleteLineTriggerMs_ = 0;
                    lastLeaderTriggerMs_ = 0;
                    lastNewTabTriggerMs_ = 0;
                    setLeaderMode(false);
                    tabLeaderMode_ = false;
                    if (onSearchTriggered) {
                        onSearchTriggered();
                    }
                    event->accept();
                    return;
                }
            }
            lastSearchTriggerMs_ = (event->text() == firstChar) ? now : 0;
        } else {
            lastSearchTriggerMs_ = 0;
        }

        QTextCursor cursor = textCursor();

        // 3. Single-keystroke removal of a divider block on Backspace/Delete
        if (!cursor.hasSelection()) {
            if (event->key() == Qt::Key_Backspace) {
                if (isDividerBlock(cursor.block())) {
                    removeDividerBlock(cursor.block());
                    event->accept();
                    return;
                }
                if (cursor.atBlockStart() && blockSizeTier(cursor.block()) != 0) {
                    suppressCursorSync_ = true;
                    QTextCursor editCursor(document());
                    editCursor.beginEditBlock();
                    applyTierToBlock(cursor.block(), 0);
                    editCursor.endEditBlock();
                    activeFontSize_ = pixelSizeForTier(0);
                    applyTypingFormatToCursor();
                    suppressCursorSync_ = false;
                    if (onFormattingChanged) {
                        onFormattingChanged();
                    }
                    event->accept();
                    return;
                }
                if (cursor.atBlockStart() && cursor.block().previous().isValid() &&
                    isDividerBlock(cursor.block().previous())) {
                    removeDividerBlock(cursor.block().previous());
                    event->accept();
                    return;
                }
            } else if (event->key() == Qt::Key_Delete) {
                if (isDividerBlock(cursor.block())) {
                    removeDividerBlock(cursor.block());
                    event->accept();
                    return;
                }
                if (cursor.atBlockEnd() && cursor.block().next().isValid() &&
                    isDividerBlock(cursor.block().next())) {
                    removeDividerBlock(cursor.block().next());
                    event->accept();
                    return;
                }
            }
        }

        // 4. Prevent typing inside an existing divider block; step onto a new line first
        if (!cursor.hasSelection() && isDividerBlock(cursor.block())) {
            if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
                cursor.beginEditBlock();
                if (cursor.atBlockStart()) {
                    cursor.insertBlock();
                    cursor.movePosition(QTextCursor::PreviousBlock);
                } else {
                    cursor.movePosition(QTextCursor::EndOfBlock);
                    cursor.insertBlock();
                }
                QTextCharFormat fmt = currentTypingCharFormat();
                cursor.setCharFormat(fmt);
                cursor.endEditBlock();
                setTextCursor(cursor);
                applyTypingFormatToCursor();
                event->accept();
                return;
            }
            if (!event->text().isEmpty() && noModifiers) {
                cursor.beginEditBlock();
                cursor.movePosition(QTextCursor::EndOfBlock);
                cursor.insertBlock();
                QTextCharFormat fmt = currentTypingCharFormat();
                cursor.setCharFormat(fmt);
                cursor.endEditBlock();
                setTextCursor(cursor);
            }
        }

        // 4B. Pressing Enter at the end of a heading/small line starts the next line as Normal text
        if (!cursor.hasSelection() &&
            !isDividerBlock(cursor.block()) &&
            (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
            noModifiers &&
            cursor.atBlockEnd() &&
            blockSizeTier(cursor.block()) != 0) {
            suppressCursorSync_ = true;
            cursor.beginEditBlock();
            QTextBlockFormat normalBfmt;
            normalBfmt.setHeadingLevel(0);
            normalBfmt.setProperty(kSmallBlockProperty, false);
            activeFontSize_ = pixelSizeForTier(0);
            QTextCharFormat normalCfmt = currentTypingCharFormat();
            cursor.insertBlock(normalBfmt, normalCfmt);
            cursor.setCharFormat(normalCfmt);
            cursor.endEditBlock();
            setTextCursor(cursor);
            applyTypingFormatToCursor();
            suppressCursorSync_ = false;
            if (onToolbarSyncNeeded) {
                onToolbarSyncNeeded();
            }
            event->accept();
            return;
        }

        // 5. Ensure typed characters always use the currently selected typing color/bold/italic/size
        if (!event->text().isEmpty() && noModifiers) {
            applyTypingFormatToCursor();
        }

        QTextEdit::keyPressEvent(event);

        // 6A. Auto-convert "# ", "## ", "### " typed at the start of a line into Markdown headings
        if (event->text() == QLatin1String(" ") && noModifiers) {
            QTextCursor after = textCursor();
            QTextBlock blk = after.block();
            if (!after.hasSelection() && !isDividerBlock(blk)) {
                int posInBlk = after.positionInBlock();
                QString prefix = blk.text().left(posInBlk);
                int targetTier = 0;
                if (prefix == QLatin1String("# ")) targetTier = 3;
                else if (prefix == QLatin1String("## ")) targetTier = 2;
                else if (prefix == QLatin1String("### ")) targetTier = 1;

                if (targetTier > 0) {
                    suppressCursorSync_ = true;
                    after.beginEditBlock();
                    after.movePosition(QTextCursor::StartOfBlock);
                    after.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, prefix.size());
                    after.removeSelectedText();
                    applyTierToBlock(after.block(), targetTier);
                    after.endEditBlock();
                    setTextCursor(after);
                    activeFontSize_ = pixelSizeForTier(targetTier);
                    applyTypingFormatToCursor();
                    suppressCursorSync_ = false;
                    if (onFormattingChanged) {
                        onFormattingChanged();
                    }
                    return;
                }
            }
        }

        // 6B. Check if the user just finished typing the divider trigger (default "---")
        const QString &divTrig = keys_.dividerTrigger;
        if (!divTrig.isEmpty() && !event->text().isEmpty() &&
            divTrig.endsWith(event->text())) {
            QTextCursor after = textCursor();
            QTextBlock blk = after.block();
            int posInBlk = after.positionInBlock();
            QString prefix = blk.text().left(posInBlk);

            if (prefix.endsWith(divTrig)) {
                convertTypedTriggerToDivider(after, divTrig.size());
            }
        }
    }

    void insertFromMimeData(const QMimeData *source) override {
        if (!source || !source->hasText()) {
            return;
        }
        QTextCursor cursor = textCursor();
        cursor.insertText(source->text(), currentTypingCharFormat());
        setTextCursor(cursor);
        formatExistingDividers();
    }

private:
    QString extractTabChordKey(const QString &primary, const QString &alt, const QString &fallback) const {
        QString prefix = keys_.newTabTrigger + QLatin1String("+");
        if (primary.startsWith(prefix, Qt::CaseInsensitive) && primary.size() > prefix.size()) {
            return primary.mid(prefix.size()).trimmed();
        }
        if (alt.startsWith(prefix, Qt::CaseInsensitive) && alt.size() > prefix.size()) {
            return alt.mid(prefix.size()).trimmed();
        }
        return fallback;
    }

    static int matchColorSlot(const QColor &c, const std::array<QColor, 4> &palette) {
        if (!c.isValid()) return -1;
        for (int i = 0; i < 4; ++i) {
            if (c.rgb() == palette[i].rgb()) {
                return i;
            }
        }
        return -1;
    }

    void applySpanFontOverride(QTextCursor selectionCursor,
                               const std::function<void(bool &italic, bool &bold, int &px)> &mutator) {
        int selStart = selectionCursor.selectionStart();
        int selEnd = selectionCursor.selectionEnd();
        if (selEnd <= selStart) return;

        struct SpanMod {
            int start;
            int end;
            QColor color;
            QBrush bgBrush;
            bool italic;
            bool bold;
            int px;
        };
        QList<SpanMod> mods;

        QTextBlock block = document()->findBlock(selStart);
        while (block.isValid() && block.position() < selEnd) {
            if (!isDividerBlock(block)) {
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    QTextFragment frag = it.fragment();
                    if (!frag.isValid()) continue;
                    int fStart = frag.position();
                    int fEnd = fStart + frag.length();
                    int oStart = std::max(selStart, fStart);
                    int oEnd = std::min(selEnd, fEnd);
                    if (oEnd > oStart) {
                        QTextCharFormat cfmt = frag.charFormat();
                        QColor col = cfmt.foreground().color();
                        if (!col.isValid() || col.alpha() == 0) col = activeColor_;
                        QBrush bg = cfmt.background();
                        QFont f = cfmt.font();
                        bool itl = f.italic();
                        bool b = f.weight() >= QFont::Bold;
                        int px = f.pixelSize() > 0 ? f.pixelSize() : activeFontSize_;
                        mutator(itl, b, px);
                        mods.append(SpanMod{oStart, oEnd, col, bg, itl, b, px});
                    }
                }
            }
            block = block.next();
        }

        QTextCursor editCursor(document());
        editCursor.beginEditBlock();
        for (const SpanMod &m : mods) {
            editCursor.setPosition(m.start);
            editCursor.setPosition(m.end, QTextCursor::KeepAnchor);
            QTextCharFormat fmt;
            fmt.setForeground(m.color);
            fmt.setBackground(m.bgBrush);
            fmt.setFont(makeFontWithFallback(
                theme_.editorFontFamily,
                m.px,
                m.bold ? QFont::Bold : theme_.editorFontWeight,
                m.italic));
            editCursor.mergeCharFormat(fmt);
        }
        editCursor.endEditBlock();
        formatExistingDividers();
    }

    struct InlineMdState {
        bool bold = false;
        bool italic = false;
        QColor fgColor;
        QColor bgColor;
        int pixelSize = 16;
    };

    void applyTierToBlock(const QTextBlock &block, int tier) {
        if (!block.isValid() || isDividerBlock(block)) {
            return;
        }
        int clamped = std::clamp(tier, -1, 3);
        int px = pixelSizeForTier(clamped);

        QTextCursor c(block);
        QTextBlockFormat bfmt = block.blockFormat();
        bfmt.setHeadingLevel(clamped == 3 ? 1 : (clamped == 2 ? 2 : (clamped == 1 ? 3 : 0)));
        bfmt.setProperty(kSmallBlockProperty, clamped == -1);
        c.setBlockFormat(bfmt);

        QTextCharFormat bcf = block.charFormat();
        QFont bf = bcf.font();
        bcf.setFont(makeFontWithFallback(
            theme_.editorFontFamily,
            px,
            bf.weight() >= QFont::Bold ? QFont::Bold : theme_.editorFontWeight,
            bf.italic()));
        c.setBlockCharFormat(bcf);

        for (auto it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            QTextCharFormat curFmt = frag.charFormat();
            QFont f = curFmt.font();
            QTextCharFormat upd;
            upd.setFont(makeFontWithFallback(
                theme_.editorFontFamily,
                px,
                f.weight() >= QFont::Bold ? QFont::Bold : theme_.editorFontWeight,
                f.italic()));
            c.setPosition(frag.position());
            c.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
            c.mergeCharFormat(upd);
        }
    }

    static QString decodeHtmlEntities(const QString &s) {
        QString out = s;
        out.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
        out.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
        out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        return out;
    }

    static QColor parseCssColorProp(const QString &styleAttr, const QStringList &propNames) {
        for (const QString &prop : propNames) {
            QRegularExpression re(
                QStringLiteral("(?:^|;)\\s*%1\\s*:\\s*([^;\"']+)").arg(QRegularExpression::escape(prop)),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch m = re.match(styleAttr);
            if (m.hasMatch()) {
                QColor c(m.captured(1).trimmed());
                if (c.isValid()) {
                    return c;
                }
            }
        }
        return {};
    }

    static std::pair<int, int> findClosingHtmlTag(const QString &text, int searchFrom, const QString &tagName) {
        QRegularExpression tokenRe(
            QStringLiteral("<(/?)%1(?:\\s+[^>]*)?>").arg(QRegularExpression::escape(tagName)),
            QRegularExpression::CaseInsensitiveOption);
        int depth = 1;
        int pos = searchFrom;
        while (pos < text.size()) {
            QRegularExpressionMatch m = tokenRe.match(text, pos);
            if (!m.hasMatch()) {
                return {-1, -1};
            }
            bool isClose = !m.captured(1).isEmpty();
            if (isClose) {
                --depth;
                if (depth == 0) {
                    return {static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedEnd())};
                }
            } else {
                ++depth;
            }
            pos = static_cast<int>(m.capturedEnd());
        }
        return {-1, -1};
    }

    void emitMarkdownChunk(QTextCursor &cursor, const QString &raw, const InlineMdState &state) const {
        if (raw.isEmpty()) {
            return;
        }
        QTextCharFormat fmt;
        if (state.bgColor.isValid()) {
            fmt.setBackground(state.bgColor);
            fmt.setForeground(state.fgColor.isValid() ? state.fgColor : contrastTextColor(state.bgColor));
        } else {
            fmt.setBackground(Qt::NoBrush);
            fmt.setForeground(state.fgColor.isValid() ? state.fgColor : theme_.textColors[0]);
        }
        fmt.setFont(makeFontWithFallback(
            theme_.editorFontFamily,
            state.pixelSize,
            state.bold ? QFont::Bold : theme_.editorFontWeight,
            state.italic));
        cursor.insertText(decodeHtmlEntities(raw), fmt);
    }

    void parseMarkdownInline(QTextCursor &cursor, const QString &text, const InlineMdState &state) const {
        static const QRegularExpression kOpenTagRe(
            QStringLiteral("^<(span|mark|small|b|strong|i|em)(?:\\s+([^>]*))?>"),
            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression kStyleAttrRe(
            QStringLiteral("style\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)')"),
            QRegularExpression::CaseInsensitiveOption);

        auto isWordChar = [](QChar ch) {
            return ch.isLetterOrNumber() || ch == QLatin1Char('_');
        };

        QString plainBuf;
        auto flushPlain = [&]() {
            if (!plainBuf.isEmpty()) {
                emitMarkdownChunk(cursor, plainBuf, state);
                plainBuf.clear();
            }
        };

        const int len = text.size();
        int i = 0;
        while (i < len) {
            // 1. Backslash escapes (\*, \_, \\)
            if (text[i] == QLatin1Char('\\') && i + 1 < len &&
                (text[i + 1] == QLatin1Char('*') || text[i + 1] == QLatin1Char('_') || text[i + 1] == QLatin1Char('\\'))) {
                plainBuf.append(text[i + 1]);
                i += 2;
                continue;
            }

            // 2. Inline HTML tags: <span>, <mark>, <small>, <b>, <strong>, <i>, <em>
            if (text[i] == QLatin1Char('<')) {
                QRegularExpressionMatch tm = kOpenTagRe.match(text.mid(i));
                if (tm.hasMatch()) {
                    QString tag = tm.captured(1).toLower();
                    QString attrs = tm.captured(2);
                    int afterOpen = i + static_cast<int>(tm.capturedLength());
                    auto [closeStart, closeEnd] = findClosingHtmlTag(text, afterOpen, tag);
                    if (closeStart >= afterOpen) {
                        flushPlain();
                        InlineMdState next = state;
                        if (tag == QLatin1String("b") || tag == QLatin1String("strong")) {
                            next.bold = true;
                        } else if (tag == QLatin1String("i") || tag == QLatin1String("em")) {
                            next.italic = true;
                        } else if (tag == QLatin1String("small")) {
                            next.pixelSize = pixelSizeForTier(-1);
                        } else if (tag == QLatin1String("span") || tag == QLatin1String("mark")) {
                            QString styleVal;
                            QRegularExpressionMatch sm = kStyleAttrRe.match(attrs);
                            if (sm.hasMatch()) {
                                styleVal = !sm.captured(1).isEmpty() ? sm.captured(1) : sm.captured(2);
                            }
                            QColor bg = parseCssColorProp(styleVal, {QStringLiteral("background-color"), QStringLiteral("background")});
                            QColor fg = parseCssColorProp(styleVal, {QStringLiteral("color")});
                            if (tag == QLatin1String("mark")) {
                                next.bgColor = bg.isValid() ? bg : theme_.textColors[2];
                                next.fgColor = fg.isValid() ? fg : contrastTextColor(next.bgColor);
                            } else {
                                if (bg.isValid()) {
                                    next.bgColor = bg;
                                    next.fgColor = fg.isValid() ? fg : contrastTextColor(bg);
                                } else if (fg.isValid()) {
                                    next.fgColor = fg;
                                }
                            }
                        }
                        parseMarkdownInline(cursor, text.mid(afterOpen, closeStart - afterOpen), next);
                        i = closeEnd;
                        continue;
                    }
                }
            }

            // 3. Obsidian highlight ==text==
            if (i + 1 < len && text[i] == QLatin1Char('=') && text[i + 1] == QLatin1Char('=') &&
                i + 2 < len && !text[i + 2].isSpace()) {
                int j = text.indexOf(QStringLiteral("=="), i + 2);
                if (j > i + 2 && !text[j - 1].isSpace()) {
                    flushPlain();
                    InlineMdState next = state;
                    next.bgColor = theme_.textColors[2];
                    next.fgColor = contrastTextColor(next.bgColor);
                    parseMarkdownInline(cursor, text.mid(i + 2, j - (i + 2)), next);
                    i = j + 2;
                    continue;
                }
            }

            // 4. ***bold+italic***
            if (i + 2 < len && text[i] == QLatin1Char('*') && text[i + 1] == QLatin1Char('*') && text[i + 2] == QLatin1Char('*') &&
                i + 3 < len && !text[i + 3].isSpace()) {
                int j = text.indexOf(QStringLiteral("***"), i + 3);
                while (j != -1 && (text[j - 1].isSpace() || text[j - 1] == QLatin1Char('\\'))) {
                    j = text.indexOf(QStringLiteral("***"), j + 1);
                }
                if (j > i + 3) {
                    flushPlain();
                    InlineMdState next = state;
                    next.bold = true;
                    next.italic = true;
                    parseMarkdownInline(cursor, text.mid(i + 3, j - (i + 3)), next);
                    i = j + 3;
                    continue;
                }
            }

            // 5. **bold**
            if (i + 1 < len && text[i] == QLatin1Char('*') && text[i + 1] == QLatin1Char('*') &&
                i + 2 < len && !text[i + 2].isSpace()) {
                int j = text.indexOf(QStringLiteral("**"), i + 2);
                while (j != -1 && (text[j - 1].isSpace() || text[j - 1] == QLatin1Char('\\'))) {
                    j = text.indexOf(QStringLiteral("**"), j + 1);
                }
                if (j > i + 2) {
                    flushPlain();
                    InlineMdState next = state;
                    next.bold = true;
                    parseMarkdownInline(cursor, text.mid(i + 2, j - (i + 2)), next);
                    i = j + 2;
                    continue;
                }
            }

            // 6. *italic*
            if (text[i] == QLatin1Char('*') &&
                (i + 1 < len && text[i + 1] != QLatin1Char('*') && !text[i + 1].isSpace())) {
                int j = -1;
                for (int k = i + 1; k < len; ++k) {
                    if (text[k] == QLatin1Char('*') && text[k - 1] != QLatin1Char('\\')) {
                        bool isDouble = (k + 1 < len && text[k + 1] == QLatin1Char('*'));
                        if (isDouble) {
                            ++k; // skip ** inside *...*
                            continue;
                        }
                        if (!text[k - 1].isSpace()) {
                            j = k;
                            break;
                        }
                    }
                }
                if (j > i + 1) {
                    flushPlain();
                    InlineMdState next = state;
                    next.italic = true;
                    parseMarkdownInline(cursor, text.mid(i + 1, j - (i + 1)), next);
                    i = j + 1;
                    continue;
                }
            }

            // 7. __bold__ and _italic_ (only at word boundaries so snake_case stays literal)
            if (text[i] == QLatin1Char('_') && (i == 0 || !isWordChar(text[i - 1]))) {
                if (i + 1 < len && text[i + 1] == QLatin1Char('_') &&
                    i + 2 < len && !text[i + 2].isSpace()) {
                    int j = text.indexOf(QStringLiteral("__"), i + 2);
                    while (j != -1 && (text[j - 1].isSpace() || (j + 2 < len && isWordChar(text[j + 2])))) {
                        j = text.indexOf(QStringLiteral("__"), j + 1);
                    }
                    if (j > i + 2) {
                        flushPlain();
                        InlineMdState next = state;
                        next.bold = true;
                        parseMarkdownInline(cursor, text.mid(i + 2, j - (i + 2)), next);
                        i = j + 2;
                        continue;
                    }
                } else if (i + 1 < len && text[i + 1] != QLatin1Char('_') && !text[i + 1].isSpace()) {
                    int j = -1;
                    for (int k = i + 1; k < len; ++k) {
                        if (text[k] == QLatin1Char('_') && text[k - 1] != QLatin1Char('\\') &&
                            !text[k - 1].isSpace() && (k + 1 >= len || !isWordChar(text[k + 1]))) {
                            j = k;
                            break;
                        }
                    }
                    if (j > i + 1) {
                        flushPlain();
                        InlineMdState next = state;
                        next.italic = true;
                        parseMarkdownInline(cursor, text.mid(i + 1, j - (i + 1)), next);
                        i = j + 1;
                        continue;
                    }
                }
            }

            plainBuf.append(text[i]);
            ++i;
        }
        flushPlain();
    }

    void applyThemeStyle() {
        QString sheet = QString(R"(
            QTextEdit, QTextEdit:focus {
                background-color: %1;
                color: %2;
                border: 0px solid transparent;
                outline: none;
                selection-background-color: %3;
                selection-color: %4;
            }
            QScrollBar:vertical {
                background: %1;
                width: 6px;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background: %5;
                min-height: 20px;
                border-radius: 3px;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
        )")
                            .arg(theme_.background.name(QColor::HexRgb),
                                 theme_.textColors[0].name(QColor::HexRgb),
                                 theme_.selectionBackground.name(QColor::HexRgb),
                                 theme_.selectionText.name(QColor::HexRgb),
                                 theme_.scrollbarColor.name(QColor::HexRgb));
        setStyleSheet(sheet);
    }

    void setLeaderMode(bool active) {
        if (leaderMode_ != active) {
            leaderMode_ = active;
            if (onLeaderModeChanged) {
                onLeaderModeChanged(leaderMode_);
            }
        }
    }

    bool isDividerBlock(const QTextBlock &block) const {
        if (!block.isValid()) return false;
        QString txt = block.text();
        return txt == QLatin1String("---") ||
               (!keys_.dividerTrigger.isEmpty() && txt == keys_.dividerTrigger);
    }

    void removeDividerBlock(const QTextBlock &block) {
        if (!block.isValid()) {
            return;
        }
        QTextCursor c(block);
        c.beginEditBlock();
        c.movePosition(QTextCursor::StartOfBlock);
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        c.removeSelectedText();
        if (c.block().next().isValid()) {
            c.deleteChar();
        } else if (c.block().previous().isValid()) {
            c.deletePreviousChar();
        }
        c.endEditBlock();
        setTextCursor(c);
        applyTypingFormatToCursor();
        viewport()->update();
    }

    void convertTypedTriggerToDivider(QTextCursor cursor, int triggerLen) {
        cursor.beginEditBlock();

        for (int i = 0; i < triggerLen; ++i) {
            cursor.deletePreviousChar();
        }

        if (cursor.positionInBlock() > 0) {
            cursor.insertBlock();
        }

        QTextBlockFormat divBfmt;
        divBfmt.setHeadingLevel(0);
        divBfmt.setProperty(kSmallBlockProperty, false);
        cursor.setBlockFormat(divBfmt);

        QTextCharFormat divFmt;
        divFmt.setForeground(Qt::transparent);
        cursor.insertText(QStringLiteral("---"), divFmt);

        activeFontSize_ = pixelSizeForTier(0);
        QTextCharFormat textFmt = currentTypingCharFormat();
        cursor.insertBlock(divBfmt, textFmt);
        cursor.setBlockCharFormat(textFmt);
        cursor.setCharFormat(textFmt);

        cursor.endEditBlock();
        setTextCursor(cursor);
        applyTypingFormatToCursor();
        if (onToolbarSyncNeeded) {
            onToolbarSyncNeeded();
        }
        viewport()->update();
    }

    QTextCharFormat currentTypingCharFormat() const {
        QTextCharFormat fmt;
        if (highlightMode_) {
            fmt.setBackground(activeColor_);
            fmt.setForeground(contrastTextColor(activeColor_));
        } else {
            fmt.setBackground(Qt::NoBrush);
            fmt.setForeground(activeColor_);
        }
        fmt.setFont(makeFontWithFallback(
            theme_.editorFontFamily,
            activeFontSize_,
            activeBold_ ? QFont::Bold : theme_.editorFontWeight,
            activeItalic_));
        return fmt;
    }

    void applyTypingFormatToCursor() {
        mergeCurrentCharFormat(currentTypingCharFormat());
    }

    ThemeConfig theme_;
    KeybindingsConfig keys_;
    QColor activeColor_;
    int activeColorIndex_ = 0;
    bool activeBold_ = false;
    bool activeItalic_ = false;
    bool highlightMode_ = false;
    int activeFontSize_ = 16;
    qint64 lastLeaderTriggerMs_ = 0;
    qint64 lastNewTabTriggerMs_ = 0;
    qint64 lastDeleteLineTriggerMs_ = 0;
    qint64 lastSearchTriggerMs_ = 0;
    bool leaderMode_ = false;
    bool tabLeaderMode_ = false;
    bool suppressCursorSync_ = false;
};

// Bottom-left formatting icon button matching exact Figma SVG icons:
// icon_bold (9x12), icon_italic (12x12), icon_up (9x12), icon_down (8x12)
class FormatIconButton : public QWidget {
public:
    enum class IconType {
        Bold,
        Italic,
        ArrowUp,
        ArrowDown,
        Highlight,
        Search
    };

    FormatIconButton(IconType type, const ThemeConfig &theme, QWidget *parent = nullptr)
        : QWidget(parent), type_(type), theme_(theme) {
        if (type_ == IconType::Bold) {
            glyphSize_ = QSize(9, 12);
            svgPath_ = resolveAssetSvgPath(QStringLiteral("icon_bold.svg"));
        } else if (type_ == IconType::Italic) {
            glyphSize_ = QSize(12, 12);
            svgPath_ = resolveAssetSvgPath(QStringLiteral("icon_italic.svg"));
        } else if (type_ == IconType::ArrowUp) {
            glyphSize_ = QSize(9, 12);
            svgPath_ = resolveAssetSvgPath(QStringLiteral("icon_up.svg"));
        } else if (type_ == IconType::ArrowDown) {
            glyphSize_ = QSize(8, 12);
            svgPath_ = resolveAssetSvgPath(QStringLiteral("icon_down.svg"));
        } else {
            glyphSize_ = QSize(14, 14);
        }
#ifdef Q_OS_ANDROID
        setFixedSize(28, 28);
        glyphSize_ = QSize(qRound(glyphSize_.width() * 1.5), qRound(glyphSize_.height() * 1.5));
#else
        setFixedSize(glyphSize_);
#endif
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setMouseTracking(true);
    }

    std::function<void()> onClicked;

    void applyTheme(const ThemeConfig &theme) {
        theme_ = theme;
        update();
    }

    void setActive(bool active) {
        if (active_ != active) {
            active_ = active;
            update();
        }
    }

    void setFormatModeHighlight(bool highlighted) {
        if (highlighted_ != highlighted) {
            highlighted_ = highlighted;
            update();
        }
    }

protected:
    void enterEvent(QEnterEvent *) override {
        hovered_ = true;
        update();
    }

    void leaveEvent(QEvent *) override {
        hovered_ = false;
        update();
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && onClicked) {
            onClicked();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QColor iconColor;
        if (highlighted_ || active_) {
            iconColor = theme_.activeToolColor;
        } else if (hovered_) {
            iconColor = theme_.hoverTabColor;
        } else {
            iconColor = theme_.activeTabColor;
        }

        if (!svgPath_.isEmpty()) {
            QRect target((width() - glyphSize_.width()) / 2,
                         (height() - glyphSize_.height()) / 2,
                         glyphSize_.width(),
                         glyphSize_.height());
            renderTintedSvg(p, svgPath_, target, iconColor, devicePixelRatioF());
        } else if (type_ == IconType::Highlight) {
            // Simple crisp highlighter square-marker glyph for mobile touch toolbar
            QRectF box((width() - 16.0) / 2.0, (height() - 16.0) / 2.0, 16.0, 16.0);
            p.setPen(QPen(iconColor, 1.8));
            p.setBrush(active_ ? iconColor : Qt::NoBrush);
            p.drawRoundedRect(box, 3.0, 3.0);
        } else if (type_ == IconType::Search) {
            // Simple crisp magnifying-glass glyph for mobile quick search
            p.setPen(QPen(iconColor, 1.8, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            QPointF center(width() / 2.0 - 2.0, height() / 2.0 - 2.0);
            p.drawEllipse(center, 5.5, 5.5);
            p.drawLine(QPointF(center.x() + 4.0, center.y() + 4.0),
                       QPointF(center.x() + 8.5, center.y() + 8.5));
        }
    }

private:
    IconType type_;
    ThemeConfig theme_;
    QString svgPath_;
    QSize glyphSize_{12, 12};
    bool active_ = false;
    bool hovered_ = false;
    bool highlighted_ = false;
};

// 12x12 circular color button matching Figma nodes 1:4, 1:5, 1:7, 1:9
class ColorDotButton : public QWidget {
public:
    ColorDotButton(const QColor &color, const QColor &bgColor, QWidget *parent = nullptr)
        : QWidget(parent), color_(color), bgColor_(bgColor) {
#ifdef Q_OS_ANDROID
        setFixedSize(24, 24);
#else
        setFixedSize(12, 12);
#endif
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
    }

    std::function<void()> onClicked;

    void setColors(const QColor &color, const QColor &bgColor) {
        color_ = color;
        bgColor_ = bgColor;
        update();
    }

    void setColorModeHighlight(bool highlighted) {
        if (highlighted_ != highlighted) {
            highlighted_ = highlighted;
            update();
        }
    }

    void setSquareMode(bool square) {
        if (squareMode_ != square) {
            squareMode_ = square;
            update();
        }
    }

    void setActiveSwatch(bool active) {
        if (activeSwatch_ != active) {
            activeSwatch_ = active;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(color_);

        const qreal sz = static_cast<qreal>(width());
        if (squareMode_) {
            p.drawRoundedRect(QRectF(0.0, 0.0, sz, sz), sz * 0.125, sz * 0.125);
        } else {
            p.drawEllipse(QRectF(0.0, 0.0, sz, sz));
        }

        // Small swatch_marker (#212421) circle in the center of the active swatch in both modes
        if (activeSwatch_) {
            p.setPen(Qt::NoPen);
            p.setBrush(bgColor_);
            const qreal dot = sz / 3.0;
            p.drawEllipse(QRectF(dot, dot, dot, dot));
        }

        // Subtle inner ring indicator when 'jj' leader mode is waiting for a key
        if (highlighted_) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(bgColor_, 1.6));
            const qreal inset = sz / 6.0;
            const qreal inner = sz - 2.0 * inset;
            if (squareMode_) {
                p.drawRoundedRect(QRectF(inset, inset, inner, inner), 1.0, 1.0);
            } else {
                p.drawEllipse(QRectF(inset, inset, inner, inner));
            }
        }
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && onClicked) {
            onClicked();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

private:
    QColor color_;
    QColor bgColor_;
    bool highlighted_ = false;
    bool squareMode_ = false;
    bool activeSwatch_ = false;
};

// Custom top header bar matching Figma wireframe (pad_frame=20, header height=17, total height=37)
class HeaderTabBar : public QWidget {
public:
    explicit HeaderTabBar(const ThemeConfig &theme, QWidget *parent = nullptr)
        : QWidget(parent), theme_(theme) {
#ifdef Q_OS_ANDROID
        setFixedHeight(48);
#else
        setFixedHeight(37);
#endif
        setMouseTracking(true);
        addIconPath_ = resolveAssetSvgPath(QStringLiteral("icon_add.svg"));

        inlineEditor_ = new QLineEdit(this);
        inlineEditor_->hide();
        inlineEditor_->setFrame(false);
        inlineEditor_->installEventFilter(this);
        connect(inlineEditor_, &QLineEdit::editingFinished, this, [this]() {
            finishRename(true);
        });

        applyTheme(theme_);
    }

    std::function<void(int)> onTabSelected;
    std::function<void(int)> onTabClosed;
    std::function<void(int, const QString &)> onTabRenamed;
    std::function<void()> onNewTabRequested;

    void applyTheme(const ThemeConfig &theme) {
        theme_ = theme;
        font_ = makeFontWithFallback(
            theme_.headerFontFamily, theme_.headerFontSize, theme_.headerFontWeight);
        inlineEditor_->setFont(font_);
        inlineEditor_->setStyleSheet(QString(R"(
            QLineEdit {
                background-color: %1;
                color: %2;
                border: 1px solid %3;
                border-radius: 2px;
                padding: 0px 2px;
                selection-background-color: %4;
            }
        )")
                                         .arg(theme_.selectionBackground.name(QColor::HexRgb),
                                              theme_.activeTabColor.name(QColor::HexRgb),
                                              theme_.plusHoverColor.name(QColor::HexRgb),
                                              theme_.selectionBackground.name(QColor::HexRgb)));
        update();
    }

    void setTabs(const QStringList &titles, int currentIndex) {
        titles_ = titles;
        currentIndex_ = currentIndex;
        update();
    }

    void setCurrentIndex(int index) {
        if (currentIndex_ != index) {
            currentIndex_ = index;
            update();
        }
    }

    void setTabTitle(int index, const QString &title) {
        if (index >= 0 && index < titles_.size()) {
            titles_[index] = title;
            update();
        }
    }

    void startRename(int index) {
        if (index < 0 || index >= titles_.size()) {
            return;
        }
        QList<TabLayout> layouts = computeLayouts();
        if (index >= layouts.size()) {
            return;
        }
        editingIndex_ = index;
        activeRename_ = true;

        QRect r = layouts[index].textRect.adjusted(-2, -2, 24, 2);
        inlineEditor_->setGeometry(r);
        inlineEditor_->setText(titles_[index]);
        inlineEditor_->selectAll();
        inlineEditor_->show();
        inlineEditor_->raise();
        inlineEditor_->setFocus();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), theme_.background);
        p.setFont(font_);

        QList<TabLayout> layouts = computeLayouts();
        QFont inactiveFont = makeFontWithFallback(
            theme_.headerFontFamily, theme_.headerFontSize, QFont::Normal);

        for (int i = 0; i < layouts.size(); ++i) {
            const TabLayout &tl = layouts[i];
            bool isSelected = (i == currentIndex_);
            bool isHovered = (i == hoveredTabIndex_);

            p.setFont(isSelected ? font_ : inactiveFont);
            QColor textColor = isSelected
                                   ? theme_.activeTabColor
                                   : (isHovered ? theme_.hoverTabColor
                                                : theme_.inactiveTabColor);
            p.setPen(textColor);
            p.drawText(tl.textRect, Qt::AlignLeft | Qt::AlignVCenter, elideTabLabel(titles_[i]));

#ifdef Q_OS_ANDROID
            if (isSelected || isHovered) {
                p.setFont(font_);
                QColor closeColor = closeHovered_ ? theme_.activeTabColor : theme_.hoverTabColor;
                p.setPen(closeColor);
                p.drawText(tl.closeRect, Qt::AlignCenter, QStringLiteral("×"));
            }
#else
            if (isHovered) {
                p.setFont(font_);
                QColor closeColor = closeHovered_ ? theme_.activeTabColor : theme_.hoverTabColor;
                p.setPen(closeColor);
                p.drawText(tl.closeRect, Qt::AlignCenter, QStringLiteral("×"));
            }
#endif
        }

        QRect plusR = plusButtonRect();
        QColor plusTint = plusHovered_ ? theme_.plusHoverColor : theme_.plusColor;
        if (!addIconPath_.isEmpty()) {
            renderTintedSvg(p, addIconPath_, plusR, plusTint, devicePixelRatioF());
        } else {
            p.setFont(font_);
            p.setPen(plusTint);
            p.drawText(plusR, Qt::AlignCenter, QStringLiteral("+"));
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        QPoint pos = event->pos();
        bool newPlusHover = plusHitRect().contains(pos);
        int newHoveredTab = -1;
        bool newCloseHover = false;

        QList<TabLayout> layouts = computeLayouts();
        for (int i = 0; i < layouts.size(); ++i) {
            if (layouts[i].hitRect.contains(pos)) {
                newHoveredTab = i;
                newCloseHover = layouts[i].closeRect.contains(pos);
                break;
            }
        }

        if (newPlusHover || newHoveredTab >= 0) {
            setCursor(Qt::PointingHandCursor);
        } else {
            unsetCursor();
        }

        if (newPlusHover != plusHovered_ ||
            newHoveredTab != hoveredTabIndex_ ||
            newCloseHover != closeHovered_) {
            plusHovered_ = newPlusHover;
            hoveredTabIndex_ = newHoveredTab;
            closeHovered_ = newCloseHover;
            update();
        }
    }

    void leaveEvent(QEvent *) override {
        plusHovered_ = false;
        hoveredTabIndex_ = -1;
        closeHovered_ = false;
        unsetCursor();
        update();
    }

    void mousePressEvent(QMouseEvent *event) override {
        QPoint pos = event->pos();
        if (event->button() == Qt::LeftButton) {
            if (plusHitRect().contains(pos)) {
                if (onNewTabRequested) {
                    onNewTabRequested();
                }
                event->accept();
                return;
            }

            QList<TabLayout> layouts = computeLayouts();
            for (int i = 0; i < layouts.size(); ++i) {
                if (layouts[i].hitRect.contains(pos)) {
                    if (layouts[i].closeRect.contains(pos)) {
                        if (onTabClosed) {
                            onTabClosed(i);
                        }
                    } else if (i != currentIndex_ && onTabSelected) {
                        onTabSelected(i);
                    }
                    event->accept();
                    return;
                }
            }
        } else if (event->button() == Qt::MiddleButton) {
            QList<TabLayout> layouts = computeLayouts();
            for (int i = 0; i < layouts.size(); ++i) {
                if (layouts[i].hitRect.contains(pos)) {
                    if (onTabClosed) {
                        onTabClosed(i);
                    }
                    event->accept();
                    return;
                }
            }
        }
        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            QPoint pos = event->pos();
            if (plusHitRect().contains(pos)) {
                return;
            }
            QList<TabLayout> layouts = computeLayouts();
            for (int i = 0; i < layouts.size(); ++i) {
                if (layouts[i].textRect.contains(pos)) {
                    startRename(i);
                    event->accept();
                    return;
                }
            }
            if (onNewTabRequested) {
                onNewTabRequested();
                event->accept();
                return;
            }
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    bool event(QEvent *e) override {
        if (e->type() == QEvent::ToolTip) {
            auto *he = static_cast<QHelpEvent *>(e);
            QPoint pos = he->pos();
            if (plusHitRect().contains(pos)) {
                QToolTip::showText(he->globalPos(), QStringLiteral("New Tab (ff + t)"), this, plusHitRect());
                return true;
            }
            QList<TabLayout> layouts = computeLayouts();
            for (int i = 0; i < layouts.size(); ++i) {
                if (layouts[i].hitRect.contains(pos)) {
                    QToolTip::showText(he->globalPos(), titles_[i], this, layouts[i].hitRect);
                    return true;
                }
            }
            QToolTip::hideText();
            e->ignore();
            return true;
        }
        return QWidget::event(e);
    }

    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == inlineEditor_ && event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Escape) {
                finishRename(false);
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    struct TabLayout {
        QRect textRect;
        QRect closeRect;
        QRect hitRect;
    };

    // Figma node 10:296 ("icon_add"): 14x14 right-aligned inside 20px frame padding, vertically centered in y=20..37
    QRect plusButtonRect() const {
#ifdef Q_OS_ANDROID
        return QRect(width() - 16 - 20, 14, 20, 20);
#else
        return QRect(width() - 20 - 14, 21, 14, 14);
#endif
    }

    QRect plusHitRect() const {
#ifdef Q_OS_ANDROID
        return plusButtonRect().adjusted(-10, -10, 10, 10);
#else
        return plusButtonRect().adjusted(-6, -4, 6, 4);
#endif
    }

    static QString elideTabLabel(const QString &title) {
        constexpr int kMaxChars = 16;
        if (title.size() > kMaxChars) {
            return title.left(kMaxChars) + QStringLiteral("...");
        }
        return title;
    }

    // Figma header: x = 20, y = 20, height = 17, gap = 20px between tabs
    QList<TabLayout> computeLayouts() const {
        QList<TabLayout> list;
        QFontMetrics fm(font_);
#ifdef Q_OS_ANDROID
        int h = std::max(28, fm.height() + 6);
        int topY = (height() - h) / 2;
        int x = 16;
        int maxRight = width() - 56;
        for (int i = 0; i < titles_.size(); ++i) {
            QString display = elideTabLabel(titles_[i]);
            int textW = std::max(28, fm.horizontalAdvance(display) + 4);
            if (x + textW > maxRight && i > 0) {
                break;
            }
            QRect textR(x, topY, textW, h);
            QRect closeR(x + textW + 2, topY, 24, h);
            QRect hitR(x - 4, 4, textW + 30, height() - 8);
            list.append(TabLayout{textR, closeR, hitR});
            x += textW + 32;
        }
#else
        int h = std::max(17, fm.height());
        int x = 20;
        int maxRight = width() - 46;
        for (int i = 0; i < titles_.size(); ++i) {
            QString display = elideTabLabel(titles_[i]);
            int textW = std::max(24, fm.horizontalAdvance(display) + 2);
            if (x + textW > maxRight && i > 0) {
                break;
            }
            QRect textR(x, 20, textW, h);
            QRect closeR(x + textW + 2, 20, 14, h);
            QRect hitR(x - 2, 16, textW + 18, h + 8);
            list.append(TabLayout{textR, closeR, hitR});
            x += textW + 20;
        }
#endif
        return list;
    }

    void finishRename(bool commit) {
        if (!activeRename_) {
            return;
        }
        activeRename_ = false;
        int idx = editingIndex_;
        editingIndex_ = -1;
        QString newText = inlineEditor_->text();
        inlineEditor_->hide();

        if (commit && idx >= 0 && idx < titles_.size() && onTabRenamed) {
            onTabRenamed(idx, newText);
        }
    }

    ThemeConfig theme_;
    QFont font_;
    QString addIconPath_;
    QStringList titles_;
    int currentIndex_ = 0;
    int hoveredTabIndex_ = -1;
    bool closeHovered_ = false;
    bool plusHovered_ = false;
    QLineEdit *inlineEditor_ = nullptr;
    int editingIndex_ = -1;
    bool activeRename_ = false;
};

// Searchable quick-open overlay showing all .txt files in the Mimenotes directory (triggered by "//")
class QuickSearchOverlay : public QWidget {
public:
    struct NoteEntry {
        QString title;
        QString dateLabel;
        QString filePath;
        QString preview;
        QString fullTextLower;
    };

    explicit QuickSearchOverlay(const ThemeConfig &theme, QWidget *parent = nullptr)
        : QWidget(parent), theme_(theme) {
        hide();

        card_ = new QWidget(this);
        card_->setObjectName("searchCard");
        auto *cardLayout = new QVBoxLayout(card_);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(8);

        searchInput_ = new QLineEdit(card_);
        searchInput_->setPlaceholderText("Search notes...");
        searchInput_->setFrame(false);
        searchInput_->installEventFilter(this);

        resultsList_ = new QListWidget(card_);
        resultsList_->setFrameStyle(QFrame::NoFrame);
        resultsList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        resultsList_->setFocusPolicy(Qt::NoFocus);

        cardLayout->addWidget(searchInput_);
        cardLayout->addWidget(resultsList_);

        connect(searchInput_, &QLineEdit::textChanged, this, [this](const QString &text) {
            populateFiltered(text);
        });
        connect(resultsList_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
            activateItem(item);
        });
        connect(resultsList_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            activateItem(item);
        });

        applyTheme(theme_);
    }

    std::function<void(const QString &)> onFileSelected;
    std::function<void()> onDismissed;

    void applyTheme(const ThemeConfig &theme) {
        theme_ = theme;
        QFont inputFont = makeFontWithFallback(theme_.headerFontFamily, 14, QFont::Normal);
        QFont listFont = makeFontWithFallback(theme_.headerFontFamily, 13, QFont::Normal);
        searchInput_->setFont(inputFont);
        resultsList_->setFont(listFont);

        QColor cardBg = theme_.background.lightness() < 128
                            ? theme_.background.lighter(118)
                            : theme_.background.darker(104);
        QColor borderCol = theme_.selectionBackground.lighter(125);

        card_->setStyleSheet(QString(R"(
            QWidget#searchCard {
                background-color: %1;
                border: 1px solid %2;
                border-radius: 8px;
            }
            QLineEdit, QLineEdit:focus {
                background-color: %3;
                color: %4;
                border: 0px solid transparent;
                outline: none;
                border-radius: 4px;
                padding: 6px 8px;
                selection-background-color: %5;
            }
            QListWidget {
                background-color: transparent;
                color: %4;
                outline: none;
            }
            QListWidget::item {
                padding: 6px 8px;
                border-radius: 4px;
            }
            QListWidget::item:selected {
                background-color: %5;
                color: %4;
            }
            QListWidget::item:hover:!selected {
                background-color: %3;
            }
        )")
                                 .arg(cardBg.name(QColor::HexRgb),
                                      borderCol.name(QColor::HexRgb),
                                      theme_.background.name(QColor::HexRgb),
                                      theme_.activeTabColor.name(QColor::HexRgb),
                                      theme_.selectionBackground.name(QColor::HexRgb)));
    }

    void openForDirectory(const QString &notesDir) {
        entries_.clear();
        QDir dir(notesDir);
        QStringList files = dir.entryList(QStringList() << "*.md" << "*.txt", QDir::Files, QDir::Name | QDir::Reversed);
        for (const QString &fname : files) {
            QString fullPath = dir.filePath(fname);
            QString title = extractDisplayTitle(fullPath);
            QString dateLabel = extractDateLabel(fullPath);
            QString content;
            QFile f(fullPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&f);
                content = in.readAll();
            }
            QString preview;
            for (const QString &line : content.split('\n')) {
                QString trimmed = line.trimmed();
                if (!trimmed.isEmpty() && trimmed != QLatin1String("---")) {
                    QString cleaned = isMarkdownFile(fullPath) ? stripMarkdownForPreview(trimmed) : trimmed;
                    if (!cleaned.isEmpty()) {
                        preview = cleaned.left(48);
                        break;
                    }
                }
            }
            entries_.append(NoteEntry{title, dateLabel, fullPath, preview, content.toLower()});
        }

        if (parentWidget()) {
            setGeometry(parentWidget()->rect());
        }
        positionCard();
        searchInput_->clear();
        populateFiltered(QString());
        show();
        raise();
        searchInput_->setFocus();
    }

    void dismiss() {
        if (!isVisible()) return;
        hide();
        if (onDismissed) {
            onDismissed();
        }
    }

    void syncGeometry() {
        if (parentWidget()) {
            setGeometry(parentWidget()->rect());
            positionCard();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(0, 0, 0, 115));
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (!card_->geometry().contains(event->pos())) {
            dismiss();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == searchInput_ && event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            int key = ke->key();
            bool ctrl = ke->modifiers() & Qt::ControlModifier;

            if (key == Qt::Key_Escape) {
                dismiss();
                return true;
            }
            if (key == Qt::Key_Down || (ctrl && (key == Qt::Key_J || key == Qt::Key_N))) {
                moveSelection(1);
                return true;
            }
            if (key == Qt::Key_Up || (ctrl && (key == Qt::Key_K || key == Qt::Key_P))) {
                moveSelection(-1);
                return true;
            }
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                if (auto *item = resultsList_->currentItem()) {
                    activateItem(item);
                } else {
                    dismiss();
                }
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    void positionCard() {
        int cardW = std::min(400, std::max(240, width() - 40));
        int cardH = std::min(280, std::max(140, height() - 88));
        int x = (width() - cardW) / 2;
        int y = 44;
        card_->setGeometry(x, y, cardW, cardH);
    }

    void populateFiltered(const QString &query) {
        resultsList_->clear();
        QString q = query.trimmed().toLower();
        for (const NoteEntry &entry : entries_) {
            bool matches = q.isEmpty() ||
                           entry.title.toLower().contains(q) ||
                           entry.dateLabel.contains(q) ||
                           entry.fullTextLower.contains(q);
            if (!matches) continue;

            QString header = entry.dateLabel.isEmpty()
                                 ? entry.title
                                 : QString("%1  (%2)").arg(entry.title, entry.dateLabel);
            QString label = entry.preview.isEmpty()
                                ? header
                                : QString("%1   —   %2").arg(header, entry.preview);
            auto *item = new QListWidgetItem(label, resultsList_);
            item->setData(Qt::UserRole, entry.filePath);
            item->setToolTip(entry.filePath);
        }
        if (resultsList_->count() > 0) {
            resultsList_->setCurrentRow(0);
        }
    }

    void moveSelection(int delta) {
        int count = resultsList_->count();
        if (count <= 0) return;
        int row = resultsList_->currentRow();
        int next = (row < 0) ? 0 : (row + delta + count) % count;
        resultsList_->setCurrentRow(next);
    }

    void activateItem(QListWidgetItem *item) {
        if (!item) return;
        QString path = item->data(Qt::UserRole).toString();
        hide();
        if (onFileSelected && !path.isEmpty()) {
            onFileSelected(path);
        }
    }

    ThemeConfig theme_;
    QWidget *card_ = nullptr;
    QLineEdit *searchInput_ = nullptr;
    QListWidget *resultsList_ = nullptr;
    QList<NoteEntry> entries_;
};

class NotepadWindow : public QMainWindow {
public:
    NotepadWindow(const AppConfig &appConfig,
                  const QString &explicitNotesDir,
                  const QString &explicitConfigPath,
                  const QString &themeOverride)
        : appConfig_(appConfig),
          explicitConfigPath_(explicitConfigPath),
          themeOverride_(themeOverride) {
        setWindowTitle("Mime Notes");

        if (!explicitNotesDir.isEmpty()) {
            notesDir_ = QDir::cleanPath(expandTilde(explicitNotesDir));
        } else if (!appConfig_.notesDir.isEmpty()) {
            notesDir_ = QDir::cleanPath(appConfig_.notesDir);
        } else {
            notesDir_ = defaultNotesDirectory();
        }

        resize(appConfig_.theme.windowWidth, appConfig_.theme.windowHeight);
        ensureDirectories();

        fontPixelSize_ = appConfig_.theme.editorFontSize;
        rebuildEditorFont();

        centralRoot_ = new QWidget(this);
        centralRoot_->setObjectName("centralRoot");
        setCentralWidget(centralRoot_);

        auto *mainLayout = new QVBoxLayout(centralRoot_);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        headerBar_ = new HeaderTabBar(appConfig_.theme, centralRoot_);
        mainLayout->addWidget(headerBar_);

        auto *editorContainer = new QWidget(centralRoot_);
        auto *editorLayout = new QVBoxLayout(editorContainer);
        editorLayout->setContentsMargins(20, 20, 20, 20);
        editorLayout->setSpacing(0);

        editorStack_ = new QStackedWidget(editorContainer);
        editorLayout->addWidget(editorStack_);
        mainLayout->addWidget(editorContainer, 1);

        // Footer matching Figma node 10:263 ("footer"):
        // x = 20, y = 435 in 484x467 (20px left/right/bottom padding, height = 12)
        // - Lower-left ("format"): 4 SVG icons (9x12, 12x12, 9x12, 8x12) with 20px gap
        // - Lower-right ("swatches"): 4 swatches (12x12, 16px gap, total 96x12)
        auto *bottomBar = new QWidget(centralRoot_);
#ifdef Q_OS_ANDROID
        bottomBar->setFixedHeight(48);
        auto *bottomLayout = new QHBoxLayout(bottomBar);
        bottomLayout->setContentsMargins(16, 4, 16, 12);
        bottomLayout->setSpacing(0);

        auto *formatRow = new QWidget(bottomBar);
        formatRow->setFixedHeight(32);
        auto *formatLayout = new QHBoxLayout(formatRow);
        formatLayout->setContentsMargins(0, 0, 0, 0);
        formatLayout->setSpacing(8);
#else
        bottomBar->setFixedHeight(32); // 12px footer + 20px bottom padding
        auto *bottomLayout = new QHBoxLayout(bottomBar);
        bottomLayout->setContentsMargins(20, 0, 20, 20);
        bottomLayout->setSpacing(0);

        // Lower-left formatting icons row (gap = 20px)
        auto *formatRow = new QWidget(bottomBar);
        formatRow->setFixedHeight(12);
        auto *formatLayout = new QHBoxLayout(formatRow);
        formatLayout->setContentsMargins(0, 0, 0, 0);
        formatLayout->setSpacing(20);
#endif

        formatButtons_[0] = new FormatIconButton(FormatIconButton::IconType::Bold, appConfig_.theme, formatRow);
        formatButtons_[0]->setToolTip("Bold (jj + a)");
        formatButtons_[0]->onClicked = [this]() {
            if (auto *ed = currentEditor()) {
                ed->toggleBold();
                ed->setFocus();
            }
        };

        formatButtons_[1] = new FormatIconButton(FormatIconButton::IconType::Italic, appConfig_.theme, formatRow);
        formatButtons_[1]->setToolTip("Italic (jj + s)");
        formatButtons_[1]->onClicked = [this]() {
            if (auto *ed = currentEditor()) {
                ed->toggleItalic();
                ed->setFocus();
            }
        };

        formatButtons_[2] = new FormatIconButton(FormatIconButton::IconType::ArrowUp, appConfig_.theme, formatRow);
        formatButtons_[2]->setToolTip("Heading size up — ### / ## / # (jj + d)");
        formatButtons_[2]->onClicked = [this]() {
            if (auto *ed = currentEditor()) {
                ed->adjustTypingFontSize(appConfig_.keys.fontSizeStep);
                ed->setFocus();
            }
        };

        formatButtons_[3] = new FormatIconButton(FormatIconButton::IconType::ArrowDown, appConfig_.theme, formatRow);
        formatButtons_[3]->setToolTip("Heading size down (jj + f)");
        formatButtons_[3]->onClicked = [this]() {
            if (auto *ed = currentEditor()) {
                ed->adjustTypingFontSize(-appConfig_.keys.fontSizeStep);
                ed->setFocus();
            }
        };

        for (int i = 0; i < 4; ++i) {
            formatLayout->addWidget(formatButtons_[i]);
        }

#ifdef Q_OS_ANDROID
        mobileHighlightBtn_ = new FormatIconButton(FormatIconButton::IconType::Highlight, appConfig_.theme, formatRow);
        mobileHighlightBtn_->setToolTip("Toggle Highlighter Mode");
        mobileHighlightBtn_->onClicked = [this]() {
            if (auto *ed = currentEditor()) {
                ed->toggleHighlightMode();
                ed->setFocus();
            }
        };
        formatLayout->addWidget(mobileHighlightBtn_);

        mobileSearchBtn_ = new FormatIconButton(FormatIconButton::IconType::Search, appConfig_.theme, formatRow);
        mobileSearchBtn_->setToolTip("Search Notes");
        mobileSearchBtn_->onClicked = [this]() {
            openSearchOverlay();
        };
        formatLayout->addWidget(mobileSearchBtn_);
#endif

        // Lower-right 4 color dots (96 x 12 on desktop, 132 x 24 on Android)
        auto *colorsRow = new QWidget(bottomBar);
#ifdef Q_OS_ANDROID
        colorsRow->setFixedSize(132, 24);
        auto *colorsLayout = new QHBoxLayout(colorsRow);
        colorsLayout->setContentsMargins(0, 0, 0, 0);
        colorsLayout->setSpacing(12);
#else
        colorsRow->setFixedSize(96, 12);
        auto *colorsLayout = new QHBoxLayout(colorsRow);
        colorsLayout->setContentsMargins(0, 0, 0, 0);
        colorsLayout->setSpacing(16);
#endif

        for (int i = 0; i < 4; ++i) {
            const QColor &col = appConfig_.theme.textColors[i];
            auto *btn = new ColorDotButton(col, appConfig_.theme.background, colorsRow);
            btn->onClicked = [this, i]() {
                applyColorSlotToCurrentEditor(i);
            };
            colorButtons_[i] = btn;
            colorsLayout->addWidget(btn);
        }

        bottomLayout->addWidget(formatRow, 0, Qt::AlignLeft | Qt::AlignVCenter);
        bottomLayout->addStretch(1);
        bottomLayout->addWidget(colorsRow, 0, Qt::AlignRight | Qt::AlignVCenter);
        mainLayout->addWidget(bottomBar);

        applyWindowThemeStyle();

        // Debounced auto-save timer (300ms after typing stops)
        autoSaveTimer_ = new QTimer(this);
        autoSaveTimer_->setSingleShot(true);
        autoSaveTimer_->setInterval(300);
        connect(autoSaveTimer_, &QTimer::timeout, this, [this]() {
            saveAllDirty();
            saveSessionState();
        });

        // Header callbacks
        headerBar_->onTabSelected = [this](int index) {
            setCurrentTabIndex(index);
        };
        headerBar_->onTabClosed = [this](int index) {
            closeTab(index);
        };
        headerBar_->onTabRenamed = [this](int index, const QString &newName) {
            renameTab(index, newName);
        };
        headerBar_->onNewTabRequested = [this]() {
            createNewTab();
        };

        searchOverlay_ = new QuickSearchOverlay(appConfig_.theme, centralRoot_);
        searchOverlay_->onFileSelected = [this](const QString &filePath) {
            openFileByPath(filePath);
        };
        searchOverlay_->onDismissed = [this]() {
            if (auto *ed = currentEditor()) {
                ed->setFocus();
            }
        };

        setupShortcuts();
        loadSession();
        setupConfigWatcher();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QMainWindow::resizeEvent(event);
        if (searchOverlay_) {
            searchOverlay_->syncGeometry();
        }
    }

    void closeEvent(QCloseEvent *event) override {
        saveAllDirty();
        saveSessionState();
        QMainWindow::closeEvent(event);
    }

    bool event(QEvent *e) override {
        if (e->type() == QEvent::WindowDeactivate) {
            saveAllDirty();
            saveSessionState();
        }
        return QMainWindow::event(e);
    }

private:
    void ensureDirectories() {
        QDir dir(notesDir_);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
    }

    QString sessionFilePath() const {
        return QDir(notesDir_).filePath(".session.json");
    }

    QString trashDirPath() const {
        return QDir(notesDir_).filePath(".trash");
    }

    void rebuildEditorFont() {
        editorFont_ = makeFontWithFallback(
            appConfig_.theme.editorFontFamily,
            fontPixelSize_,
            appConfig_.theme.editorFontWeight);
    }

    void applyWindowThemeStyle() {
        if (centralRoot_) {
            centralRoot_->setStyleSheet(
                QString("QWidget#centralRoot { background-color: %1; }")
                    .arg(appConfig_.theme.background.name(QColor::HexRgb)));
        }
        if (headerBar_) {
            headerBar_->applyTheme(appConfig_.theme);
        }
        if (searchOverlay_) {
            searchOverlay_->applyTheme(appConfig_.theme);
        }
        for (int i = 0; i < 4; ++i) {
            if (colorButtons_[i]) {
                colorButtons_[i]->setColors(appConfig_.theme.textColors[i], appConfig_.theme.background);
            }
            if (formatButtons_[i]) {
                formatButtons_[i]->applyTheme(appConfig_.theme);
            }
        }
        if (mobileHighlightBtn_) {
            mobileHighlightBtn_->applyTheme(appConfig_.theme);
        }
        if (mobileSearchBtn_) {
            mobileSearchBtn_->applyTheme(appConfig_.theme);
        }
    }

    void syncFormatButtonsState() {
        NoteEditor *ed = currentEditor();
        int defaultSz = appConfig_.theme.editorFontSize;
        int curSz = ed ? ed->activeFontSize() : defaultSz;
        if (formatButtons_[0]) {
            formatButtons_[0]->setActive(ed && ed->isBoldActive());
        }
        if (formatButtons_[1]) {
            formatButtons_[1]->setActive(ed && ed->isItalicActive());
        }
        if (formatButtons_[2]) {
            formatButtons_[2]->setActive(ed && curSz > defaultSz);
        }
        if (formatButtons_[3]) {
            formatButtons_[3]->setActive(ed && curSz < defaultSz);
        }
        bool hl = ed && ed->isHighlightMode();
        if (mobileHighlightBtn_) {
            mobileHighlightBtn_->setActive(hl);
        }
        int activeSlot = ed ? ed->activeTypingColorIndex() : 0;
        for (int i = 0; i < 4; ++i) {
            if (colorButtons_[i]) {
                colorButtons_[i]->setSquareMode(hl);
                colorButtons_[i]->setActiveSwatch(i == activeSlot);
            }
        }
    }

    void setupConfigWatcher() {
        configWatcher_ = new QFileSystemWatcher(this);
        updateWatchedConfigPaths();

        connect(configWatcher_, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
            QTimer::singleShot(100, this, [this]() {
                reloadConfigAndTheme();
            });
        });
    }

    void updateWatchedConfigPaths() {
        if (!configWatcher_) return;
        QStringList current = configWatcher_->files();
        if (!current.isEmpty()) {
            configWatcher_->removePaths(current);
        }
        if (!appConfig_.configFilePath.isEmpty() &&
            !appConfig_.configFilePath.startsWith(QLatin1String(":/")) &&
            QFileInfo::exists(appConfig_.configFilePath)) {
            configWatcher_->addPath(appConfig_.configFilePath);
        }
        if (!appConfig_.themeFilePath.isEmpty() &&
            !appConfig_.themeFilePath.startsWith(QLatin1String(":/")) &&
            QFileInfo::exists(appConfig_.themeFilePath)) {
            configWatcher_->addPath(appConfig_.themeFilePath);
        }
    }

    void reloadConfigAndTheme() {
        AppConfig updated = loadAppConfig(explicitConfigPath_, themeOverride_);
        appConfig_ = updated;
        fontPixelSize_ = appConfig_.theme.editorFontSize;
        rebuildEditorFont();
        applyWindowThemeStyle();
        setupShortcuts();

        for (int i = 0; i < tabCount(); ++i) {
            if (auto *ed = editorAt(i)) {
                ed->setFont(editorFont_);
                QFontMetrics fm(editorFont_);
                ed->setTabStopDistance(fm.horizontalAdvance(' ') * 4);
                ed->updateConfig(appConfig_.theme, appConfig_.keys);
            }
        }
        syncFormatButtonsState();
        updateWatchedConfigPaths();
        saveSessionState();
    }

    void setColorDotsHighlighted(bool active) {
        for (ColorDotButton *btn : colorButtons_) {
            if (btn) {
                btn->setColorModeHighlight(active);
            }
        }
    }

    void setFormatIconsHighlighted(bool active) {
        for (FormatIconButton *btn : formatButtons_) {
            if (btn) {
                btn->setFormatModeHighlight(active);
            }
        }
    }

    void applyColorSlotToCurrentEditor(int slotIndex) {
        if (auto *ed = currentEditor()) {
            ed->setActiveTypingColorSlot(slotIndex, true);
            setDirty(ed, true);
            autoSaveTimer_->start();
            ed->setFocus();
        }
    }

    void setupShortcuts() {
        qDeleteAll(shortcuts_);
        shortcuts_.clear();

        auto bind = [this](const QString &seqStr, std::function<void()> fn) {
            QString trimmed = seqStr.trimmed();
            if (trimmed.isEmpty()) return;
            // Skip leader-chord bindings like "ff+j" / "jj+a" which are handled in NoteEditor
            if (trimmed.startsWith(appConfig_.keys.newTabTrigger + QLatin1String("+"), Qt::CaseInsensitive) ||
                trimmed.startsWith(appConfig_.keys.leaderTrigger + QLatin1String("+"), Qt::CaseInsensitive)) {
                return;
            }
            auto *sc = new QShortcut(QKeySequence(trimmed), this);
            connect(sc, &QShortcut::activated, this, std::move(fn));
            shortcuts_.append(sc);
        };

        const KeybindingsConfig &kb = appConfig_.keys;
        bind(kb.newTab, [this]() { createNewTab(); });
        bind(kb.newTabAlt, [this]() { createNewTab(); });
        bind(kb.closeTab, [this]() {
            if (tabCount() > 0) {
                closeTab(currentTabIndex());
            }
        });
        bind(kb.reopenClosedTab, [this]() { reopenClosedTab(); });
        bind(kb.save, [this]() {
            saveAllDirty();
            saveSessionState();
        });
        bind(kb.renameTab, [this]() {
            int idx = currentTabIndex();
            if (idx >= 0) {
                headerBar_->startRename(idx);
            }
        });
        bind(kb.renameTabAlt, [this]() {
            int idx = currentTabIndex();
            if (idx >= 0) {
                headerBar_->startRename(idx);
            }
        });

        bind(kb.nextTab, [this]() { cycleTab(1); });
        bind(kb.nextTabAlt, [this]() { cycleTab(1); });
        bind(kb.prevTab, [this]() { cycleTab(-1); });
        bind(kb.prevTabAlt, [this]() { cycleTab(-1); });

        for (int i = 1; i <= 9; ++i) {
            bind(QString("Alt+%1").arg(i), [this, i]() {
                int target = (i == 9) ? (tabCount() - 1) : (i - 1);
                if (target >= 0 && target < tabCount()) {
                    setCurrentTabIndex(target);
                }
            });
        }

        bind(kb.zoomIn, [this]() {
            if (auto *ed = currentEditor()) {
                ed->adjustTypingFontSize(appConfig_.keys.fontSizeStep);
            }
        });
        bind(kb.zoomOut, [this]() {
            if (auto *ed = currentEditor()) {
                ed->adjustTypingFontSize(-appConfig_.keys.fontSizeStep);
            }
        });
        bind(kb.zoomReset, [this]() {
            if (auto *ed = currentEditor()) {
                ed->resetHeadingTier();
            }
        });
    }

    int tabCount() const {
        return editorStack_->count();
    }

    int currentTabIndex() const {
        return editorStack_->currentIndex();
    }

    void setCurrentTabIndex(int index) {
        if (index < 0 || index >= tabCount()) {
            return;
        }
        saveAllDirty();
        setColorDotsHighlighted(false);
        setFormatIconsHighlighted(false);
        editorStack_->setCurrentIndex(index);
        headerBar_->setCurrentIndex(index);
        syncFormatButtonsState();
        if (!loadingSession_) {
            saveSessionState();
        }
        if (auto *ed = editorAt(index)) {
            ed->setFocus();
        }
    }

    void syncHeaderTabs() {
        QStringList titles;
        for (int i = 0; i < tabCount(); ++i) {
            titles.append(extractDisplayTitle(filePathAt(i)));
        }
        headerBar_->setTabs(titles, currentTabIndex());
    }

    void cycleTab(int delta) {
        int count = tabCount();
        if (count <= 1) {
            return;
        }
        int next = (currentTabIndex() + delta + count) % count;
        setCurrentTabIndex(next);
    }

    NoteEditor *editorAt(int index) const {
        return dynamic_cast<NoteEditor *>(editorStack_->widget(index));
    }

    NoteEditor *currentEditor() const {
        return editorAt(currentTabIndex());
    }

    QString filePathAt(int index) const {
        if (auto *ed = editorAt(index)) {
            return ed->property("noteFilePath").toString();
        }
        return {};
    }

    void setFilePathAt(int index, const QString &path) {
        if (auto *ed = editorAt(index)) {
            ed->setProperty("noteFilePath", path);
        }
    }

    bool isDirty(NoteEditor *ed) const {
        return ed && ed->property("noteDirty").toBool();
    }

    void setDirty(NoteEditor *ed, bool dirty) {
        if (ed) {
            ed->setProperty("noteDirty", dirty);
        }
    }

    QString uniqueNoteFilePath(const QString &desiredBaseName,
                               const QString &ignoreFilePath = {},
                               const QString &timestampPrefix = {}) const {
        QString base = sanitizeTitle(desiredBaseName);
        if (base.isEmpty()) {
            base = "Note";
        }
        QString prefix = timestampPrefix.isEmpty() ? makeTimestampPrefix() : timestampPrefix;
        QString ext = (!ignoreFilePath.isEmpty() && ignoreFilePath.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive))
                          ? QStringLiteral(".txt")
                          : QStringLiteral(".md");

        QDir dir(notesDir_);
        QString candidate = dir.filePath(prefix + base + ext);
        if (candidate == ignoreFilePath || !QFileInfo::exists(candidate)) {
            return candidate;
        }

        int suffix = 2;
        while (true) {
            candidate = dir.filePath(QString("%1%2 %3%4").arg(prefix, base).arg(suffix).arg(ext));
            if (candidate == ignoreFilePath || !QFileInfo::exists(candidate)) {
                return candidate;
            }
            ++suffix;
        }
    }

    QString nextDefaultNoteFilePath() const {
        QStringList openTitles;
        for (int i = 0; i < tabCount(); ++i) {
            openTitles.append(extractDisplayTitle(filePathAt(i)));
        }

        int n = 1;
        QString title;
        while (true) {
            title = QString("Note %1").arg(n);
            if (!openTitles.contains(title, Qt::CaseInsensitive)) {
                break;
            }
            ++n;
        }
        return uniqueNoteFilePath(title);
    }

    int addTabForFile(const QString &filePath,
                      const QString &initialContent,
                      bool writeImmediately,
                      int insertAt = -1,
                      const QJsonArray &styleSpans = {},
                      int activeColorSlot = 0,
                      const QColor &activeColorFallback = {},
                      bool activeBold = false,
                      bool activeItalic = false,
                      int activeFontSize = -1,
                      bool highlightMode = false) {
        auto *editor = new NoteEditor(appConfig_.theme, appConfig_.keys, editorStack_);
        editor->setFont(editorFont_);
        QFontMetrics fm(editorFont_);
        editor->setTabStopDistance(fm.horizontalAdvance(' ') * 4);
        if (isMarkdownFile(filePath)) {
            editor->importMarkdown(initialContent);
        } else {
            editor->setPlainText(initialContent);
            if (!styleSpans.isEmpty()) {
                editor->importStyleSpans(styleSpans);
            } else {
                editor->formatExistingDividers();
            }
        }
        int initSize = isMarkdownFile(filePath)
                           ? editor->activeFontSize()
                           : ((activeFontSize > 0) ? activeFontSize : appConfig_.theme.editorFontSize);
        editor->setFormattingState(activeBold, activeItalic, initSize, highlightMode);

        if (activeColorSlot >= 0 && activeColorSlot < 4) {
            editor->setActiveTypingColorSlot(activeColorSlot, false);
        } else if (activeColorFallback.isValid()) {
            editor->setActiveTypingColor(activeColorFallback, false);
        } else {
            editor->setActiveTypingColorSlot(0, false);
        }
        editor->setProperty("noteFilePath", filePath);
        editor->setProperty("noteDirty", writeImmediately);

        editor->onLeaderModeChanged = [this](bool active) {
            setColorDotsHighlighted(active);
            setFormatIconsHighlighted(active);
        };
        editor->onFormattingChanged = [this, editor]() {
            syncFormatButtonsState();
            setDirty(editor, true);
            autoSaveTimer_->start();
        };
        editor->onToolbarSyncNeeded = [this]() {
            syncFormatButtonsState();
        };
        editor->onNewTabTriggered = [this]() {
            QTimer::singleShot(0, this, [this]() {
                createNewTab();
            });
        };
        editor->onPrevTabTriggered = [this]() {
            QTimer::singleShot(0, this, [this]() {
                cycleTab(-1);
            });
        };
        editor->onNextTabTriggered = [this]() {
            QTimer::singleShot(0, this, [this]() {
                cycleTab(1);
            });
        };
        editor->onRenameTabTriggered = [this]() {
            QTimer::singleShot(0, this, [this]() {
                int idx = currentTabIndex();
                if (idx >= 0) {
                    headerBar_->startRename(idx);
                }
            });
        };
        editor->onSearchTriggered = [this]() {
            QTimer::singleShot(0, this, [this]() {
                openSearchOverlay();
            });
        };

        int idx = (insertAt >= 0 && insertAt <= tabCount())
                      ? editorStack_->insertWidget(insertAt, editor)
                      : editorStack_->addWidget(editor);

        syncHeaderTabs();

        if (writeImmediately) {
            saveEditor(editor);
        }

        connect(editor, &QTextEdit::textChanged, this, [this, editor]() {
            setDirty(editor, true);
            autoSaveTimer_->start();
        });

        return idx;
    }

    void openSearchOverlay() {
        saveAllDirty();
        if (searchOverlay_) {
            searchOverlay_->openForDirectory(notesDir_);
        }
    }

    void openFileByPath(const QString &filePath) {
        QString targetClean = QDir::cleanPath(filePath);
        for (int i = 0; i < tabCount(); ++i) {
            if (QDir::cleanPath(filePathAt(i)) == targetClean) {
                setCurrentTabIndex(i);
                return;
            }
        }
        if (QFileInfo::exists(targetClean)) {
            QString fname = QFileInfo(targetClean).fileName();
            QString content = readFileContent(targetClean);
            QJsonArray spans = savedSpansMap_[fname].toArray();
            int slot = savedActiveSlotsMap_.contains(fname) ? savedActiveSlotsMap_[fname].toInt(0) : -1;
            QColor activeCol(savedActiveColorsMap_[fname].toString());
            QJsonObject fObj = savedActiveFormatsMap_[fname].toObject();
            bool b = fObj["bold"].toBool(false);
            bool itl = fObj["italic"].toBool(false);
            int sz = fObj["size"].toInt(appConfig_.theme.editorFontSize);

            int idx = addTabForFile(targetClean, content, false, -1, spans, slot, activeCol, b, itl, sz);
            setCurrentTabIndex(idx);
            saveSessionState();
        }
    }

    void createNewTab() {
        QString path = nextDefaultNoteFilePath();
        int idx = addTabForFile(path, QString(), true);
        setCurrentTabIndex(idx);
        saveSessionState();
    }

    void renameTab(int index, const QString &requestedName) {
        if (index < 0 || index >= tabCount()) {
            return;
        }
        saveAllDirty();

        QString clean = sanitizeTitle(requestedName);
        if (clean.isEmpty()) {
            return;
        }

        QString oldPath = filePathAt(index);
        QString oldTitle = extractDisplayTitle(oldPath);
        if (clean == oldTitle) {
            return;
        }

        QString existingPrefix = extractTimestampPrefix(oldPath);
        QString newPath = uniqueNoteFilePath(clean, oldPath, existingPrefix);
        if (QFile::exists(oldPath)) {
            if (!QFile::rename(oldPath, newPath)) {
                return;
            }
        } else {
            setFilePathAt(index, newPath);
            saveEditor(editorAt(index));
        }

        QString oldFname = QFileInfo(oldPath).fileName();
        QString newFname = QFileInfo(newPath).fileName();
        if (oldFname != newFname) {
            if (savedSpansMap_.contains(oldFname)) {
                savedSpansMap_[newFname] = savedSpansMap_.take(oldFname);
            }
            if (savedActiveColorsMap_.contains(oldFname)) {
                savedActiveColorsMap_[newFname] = savedActiveColorsMap_.take(oldFname);
            }
            if (savedActiveSlotsMap_.contains(oldFname)) {
                savedActiveSlotsMap_[newFname] = savedActiveSlotsMap_.take(oldFname);
            }
            if (savedActiveFormatsMap_.contains(oldFname)) {
                savedActiveFormatsMap_[newFname] = savedActiveFormatsMap_.take(oldFname);
            }
        }

        setFilePathAt(index, newPath);
        syncHeaderTabs();
        saveSessionState();

        if (auto *ed = editorAt(index)) {
            ed->setFocus();
        }
    }

    void closeTab(int index) {
        if (index < 0 || index >= tabCount()) {
            return;
        }

        NoteEditor *ed = editorAt(index);
        saveEditor(ed);

        QString path = filePathAt(index);
        QString fname = QFileInfo(path).fileName();
        QString title = QFileInfo(path).completeBaseName();

        // Never delete or move files from the filesystem when closing a tab;
        // preserve formatting metadata so reopening via // or Ctrl+Shift+T restores everything.
        if (ed && !fname.isEmpty()) {
            QJsonArray spans = isMarkdownFile(path) ? QJsonArray{} : ed->exportStyleSpans();
            QString activeCol = ed->activeTypingColor().name(QColor::HexRgb).toUpper();
            int activeSlot = ed->activeTypingColorIndex();
            bool b = ed->isBoldActive();
            bool itl = ed->isItalicActive();
            int sz = ed->activeFontSize();

            if (isMarkdownFile(path)) {
                savedSpansMap_.remove(fname);
            } else {
                savedSpansMap_[fname] = spans;
            }
            savedActiveColorsMap_[fname] = activeCol;
            savedActiveSlotsMap_[fname] = activeSlot;
            QJsonObject fObj;
            fObj["bold"] = b;
            fObj["italic"] = itl;
            fObj["size"] = sz;
            savedActiveFormatsMap_[fname] = fObj;

            closedStack_.push(ClosedNote{
                title,
                path,
                spans,
                activeCol,
                activeSlot,
                b,
                itl,
                sz,
                index});
        }

        QWidget *w = editorStack_->widget(index);
        editorStack_->removeWidget(w);
        delete w;

        if (tabCount() == 0) {
            createNewTab();
        } else {
            syncHeaderTabs();
            syncFormatButtonsState();
            saveSessionState();
        }
    }

    void reopenClosedTab() {
        while (!closedStack_.isEmpty()) {
            ClosedNote item = closedStack_.pop();
            QString restoredPath = QDir::cleanPath(item.trashFilePath);
            if (!QFileInfo::exists(restoredPath)) {
                continue;
            }
            // If already open in a tab, just switch to it
            for (int i = 0; i < tabCount(); ++i) {
                if (QDir::cleanPath(filePathAt(i)) == restoredPath) {
                    setCurrentTabIndex(i);
                    return;
                }
            }
            QString content = readFileContent(restoredPath);
            int targetIdx = std::clamp(item.index, 0, tabCount());
            int idx = addTabForFile(
                restoredPath,
                content,
                false,
                targetIdx,
                item.styleSpans,
                item.activeColorIndex,
                QColor(item.activeColor),
                item.activeBold,
                item.activeItalic,
                item.activeFontSize);
            setCurrentTabIndex(idx);
            saveSessionState();
            return;
        }
    }

    static QString readFileContent(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        QTextStream in(&file);
        return in.readAll();
    }

    void saveEditor(NoteEditor *ed) {
        if (!ed || !isDirty(ed)) {
            return;
        }
        QString path = ed->property("noteFilePath").toString();
        if (path.isEmpty()) {
            return;
        }

        ensureDirectories();
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            if (isMarkdownFile(path)) {
                out << ed->exportMarkdown();
            } else {
                out << ed->toPlainText();
            }
            out.flush();
            if (file.commit()) {
                setDirty(ed, false);
            }
        }
    }

    void saveAllDirty() {
        if (autoSaveTimer_->isActive()) {
            autoSaveTimer_->stop();
        }
        for (int i = 0; i < tabCount(); ++i) {
            saveEditor(editorAt(i));
        }
    }

    void saveSessionState() {
        if (loadingSession_) {
            return;
        }
        ensureDirectories();

        QJsonArray tabsArray;
        for (int i = 0; i < tabCount(); ++i) {
            QString path = filePathAt(i);
            if (!path.isEmpty()) {
                QString fname = QFileInfo(path).fileName();
                tabsArray.append(fname);
                if (auto *ed = editorAt(i)) {
                    if (isMarkdownFile(path)) {
                        savedSpansMap_.remove(fname);
                    } else {
                        savedSpansMap_[fname] = ed->exportStyleSpans();
                    }
                    savedActiveColorsMap_[fname] = ed->activeTypingColor().name(QColor::HexRgb).toUpper();
                    savedActiveSlotsMap_[fname] = ed->activeTypingColorIndex();
                    QJsonObject fObj;
                    fObj["bold"] = ed->isBoldActive();
                    fObj["italic"] = ed->isItalicActive();
                    fObj["size"] = ed->activeFontSize();
                    fObj["highlight"] = ed->isHighlightMode();
                    savedActiveFormatsMap_[fname] = fObj;
                }
            }
        }

        QJsonObject root;
        root["open_tabs"] = tabsArray;
        int currentIdx = currentTabIndex();
        if (currentIdx >= 0) {
            root["active_tab"] = QFileInfo(filePathAt(currentIdx)).fileName();
        }
        root["window_width"] = width();
        root["window_height"] = height();
        root["tab_color_spans"] = savedSpansMap_;
        root["tab_active_colors"] = savedActiveColorsMap_;
        root["tab_active_color_slots"] = savedActiveSlotsMap_;
        root["tab_active_formats"] = savedActiveFormatsMap_;

        QSaveFile file(sessionFilePath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            file.commit();
        }
    }

    void loadSession() {
        loadingSession_ = true;

        QDir dir(notesDir_);
        QStringList existingFiles = dir.entryList(QStringList() << "*.md" << "*.txt", QDir::Files, QDir::Name);

        QStringList orderedFiles;
        QString activeFileName;
        bool hasSessionFile = false;

        QFile sessionFile(sessionFilePath());
        if (sessionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument doc = QJsonDocument::fromJson(sessionFile.readAll());
            if (doc.isObject()) {
                hasSessionFile = true;
                QJsonObject obj = doc.object();
                int w = obj["window_width"].toInt(appConfig_.theme.windowWidth);
                int h = obj["window_height"].toInt(appConfig_.theme.windowHeight);
#ifndef Q_OS_ANDROID
                resize(std::max(300, w), std::max(240, h));
#else
                Q_UNUSED(w);
                Q_UNUSED(h);
#endif

                activeFileName = obj["active_tab"].toString();
                savedSpansMap_ = obj["tab_color_spans"].toObject();
                savedActiveColorsMap_ = obj["tab_active_colors"].toObject();
                savedActiveSlotsMap_ = obj["tab_active_color_slots"].toObject();
                savedActiveFormatsMap_ = obj["tab_active_formats"].toObject();

                QJsonArray arr = obj["open_tabs"].toArray();
                for (const QJsonValue &v : arr) {
                    QString fname = v.toString();
                    if (existingFiles.contains(fname) && !orderedFiles.contains(fname)) {
                        orderedFiles.append(fname);
                    }
                }
            }
        }

        // Only auto-open all existing .txt files on first launch when no .session.json exists
        if (!hasSessionFile) {
            for (const QString &fname : existingFiles) {
                if (!orderedFiles.contains(fname)) {
                    orderedFiles.append(fname);
                }
            }
        }

        int activeIndex = 0;
        for (const QString &fname : orderedFiles) {
            QString fullPath = dir.filePath(fname);
            QString content = readFileContent(fullPath);
            QJsonArray spans = savedSpansMap_[fname].toArray();
            int slot = savedActiveSlotsMap_.contains(fname) ? savedActiveSlotsMap_[fname].toInt(0) : -1;
            QColor activeCol(savedActiveColorsMap_[fname].toString());
            QJsonObject fObj = savedActiveFormatsMap_[fname].toObject();
            bool b = fObj["bold"].toBool(false);
            bool itl = fObj["italic"].toBool(false);
            int sz = fObj["size"].toInt(appConfig_.theme.editorFontSize);
            bool hl = fObj["highlight"].toBool(false);

            int idx = addTabForFile(fullPath, content, false, -1, spans, slot, activeCol, b, itl, sz, hl);
            if (fname == activeFileName) {
                activeIndex = idx;
            }
        }

        if (tabCount() == 0) {
            QString initialPath = nextDefaultNoteFilePath();
            addTabForFile(initialPath, QString(), true);
            activeIndex = 0;
        }

        loadingSession_ = false;
        setCurrentTabIndex(activeIndex);
        syncHeaderTabs();
        syncFormatButtonsState();
        saveSessionState();
    }

    AppConfig appConfig_;
    QString explicitConfigPath_;
    QString themeOverride_;
    QString notesDir_;
    QWidget *centralRoot_ = nullptr;
    HeaderTabBar *headerBar_ = nullptr;
    QStackedWidget *editorStack_ = nullptr;
    QuickSearchOverlay *searchOverlay_ = nullptr;
    std::array<FormatIconButton *, 4> formatButtons_ = {nullptr, nullptr, nullptr, nullptr};
    FormatIconButton *mobileHighlightBtn_ = nullptr;
    FormatIconButton *mobileSearchBtn_ = nullptr;
    std::array<ColorDotButton *, 4> colorButtons_ = {nullptr, nullptr, nullptr, nullptr};
    QList<QShortcut *> shortcuts_;
    QFileSystemWatcher *configWatcher_ = nullptr;
    QTimer *autoSaveTimer_ = nullptr;
    QFont editorFont_;
    int fontPixelSize_ = 16;
    bool loadingSession_ = false;
    QStack<ClosedNote> closedStack_;
    QJsonObject savedSpansMap_;
    QJsonObject savedActiveColorsMap_;
    QJsonObject savedActiveSlotsMap_;
    QJsonObject savedActiveFormatsMap_;
};

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    QApplication::setApplicationName("mimenotes");
    QApplication::setApplicationDisplayName("Mime Notes");
    QApplication::setDesktopFileName("mimenotes");
    QApplication::setApplicationVersion("1.1.0");

    QIcon appIcon = QIcon::fromTheme("mimenotes");
    if (appIcon.isNull()) {
        QString localIcon = QDir(QCoreApplication::applicationDirPath()).filePath("mimenotes.svg");
        if (QFileInfo::exists(localIcon)) {
            appIcon = QIcon(localIcon);
        } else {
            QString userIcon = QDir::home().filePath(".local/share/icons/hicolor/scalable/apps/mimenotes.svg");
            if (QFileInfo::exists(userIcon)) {
                appIcon = QIcon(userIcon);
            } else if (QFile::exists(QStringLiteral(":/mimenotes.svg"))) {
                appIcon = QIcon(QStringLiteral(":/mimenotes.svg"));
            }
        }
    }
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription("Mime Notes — Minimalist Markdown tabbed notepad with automatic saving.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption dirOption(
        QStringList() << "d" << "dir",
        "Directory where note .md files are stored (default: Documents/Mimenotes).",
        "path");
    parser.addOption(dirOption);

    QCommandLineOption configOption(
        QStringList() << "c" << "config",
        "Path to config.json (default: ./config.json or OS user config directory).",
        "file");
    parser.addOption(configOption);

    QCommandLineOption themeOption(
        QStringList() << "t" << "theme",
        "Theme name from the themes/ folder (overrides 'theme' in config.json).",
        "name");
    parser.addOption(themeOption);

    QCommandLineOption screenshotOption(
        QStringList() << "screenshot",
        "Render window to a PNG file and exit (for visual verification).",
        "file");
    parser.addOption(screenshotOption);

    parser.addPositionalArgument("directory", "Optional notes directory path.", "[directory]");
    parser.process(app);

    QString explicitNotesDir = parser.value(dirOption);
    if (explicitNotesDir.isEmpty() && !parser.positionalArguments().isEmpty()) {
        explicitNotesDir = parser.positionalArguments().first();
    }
    if (explicitNotesDir.isEmpty()) {
        explicitNotesDir = qEnvironmentVariable("MIMENOTES_DIR");
    }
    if (explicitNotesDir.isEmpty()) {
        explicitNotesDir = qEnvironmentVariable("NOTES_DIR");
    }

    QString explicitConfigPath = parser.value(configOption);
    QString themeOverride = parser.value(themeOption);

    AppConfig appConfig = loadAppConfig(explicitConfigPath, themeOverride);

    NotepadWindow window(appConfig, explicitNotesDir, explicitConfigPath, themeOverride);
#ifdef Q_OS_ANDROID
    window.showMaximized();
#else
    window.show();
#endif

    QString screenshotPath = parser.value(screenshotOption);
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(50, &app, [&window, screenshotPath, &app]() {
            window.grab().save(screenshotPath);
            app.quit();
        });
    }

    return app.exec();
}
