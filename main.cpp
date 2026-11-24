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
#include <QShortcut>
#include <QStack>
#include <QStackedWidget>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextStream>
#include <QTimer>
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
    int windowWidth = 503;
    int windowHeight = 477;
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
    QColor inactiveTabColor = QColor(255, 255, 255, 110);
    QColor hoverTabColor = QColor(255, 255, 255, 190);
    QColor plusColor = QColor("#FFFFFF");
    QColor plusHoverColor = QColor("#B4C9AE");

    // Editor
    QString editorFontFamily = QStringLiteral("Inter");
    int editorFontSize = 16;
    QFont::Weight editorFontWeight = QFont::Normal;

    // 4 bottom-right text colors
    std::array<QColor, 4> textColors = {
        QColor("#FFFFFF"), // Color 1
        QColor("#B4C9AE"), // Color 2
        QColor("#C3B7DB"), // Color 3
        QColor("#A8C9DA")  // Color 4
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

QFont makeFontWithFallback(const QString &primaryFamily, int pixelSize, QFont::Weight weight, bool italic = false) {
    QFont font;
    QStringList families;
    if (!primaryFamily.trimmed().isEmpty()) {
        families.append(primaryFamily.trimmed());
    }
    families << QStringLiteral("Inter")
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
    QString docsPath = QDir::home().filePath("Documents");
    if (QDir(docsPath).exists()) {
        return QDir(docsPath).filePath("Mimenotes");
    }
    return QDir::home().filePath("Mimenotes");
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

QString sanitizeTitle(const QString &raw) {
    QString s = raw.trimmed();
    s.replace('/', '-');
    s.replace('\\', '-');
    s.replace('\0', "");
    while (s.startsWith('.')) {
        s.remove(0, 1);
    }
    s = s.trimmed();
    if (s.endsWith(".txt", Qt::CaseInsensitive)) {
        s.chop(4);
        s = s.trimmed();
    }
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
    candidates << QDir::current().filePath("config.json");
    candidates << QDir(QCoreApplication::applicationDirPath()).filePath("config.json");
    candidates << QDir::home().filePath(".config/mimenotes/config.json");
    candidates << QDir::home().filePath(".config/notes/config.json");

    for (const QString &cand : candidates) {
        if (QFileInfo::exists(cand)) {
            cfg.configFilePath = QFileInfo(cand).absoluteFilePath();
            break;
        }
    }

    int configDefaultFontSize = -1;
    if (!cfg.configFilePath.isEmpty()) {
        cfg.themesDirPath = QFileInfo(cfg.configFilePath).absoluteDir().filePath("themes");
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
    themeCandidates << QDir::home().filePath(".config/mimenotes/themes/" + themeNameClean + ".json");
    themeCandidates << QDir::home().filePath(".config/notes/themes/" + themeNameClean + ".json");

    for (const QString &tc : themeCandidates) {
        if (QFileInfo::exists(tc)) {
            cfg.themeFilePath = QFileInfo(tc).absoluteFilePath();
            cfg.theme = loadThemeFromFile(cfg.themeFilePath);
            break;
        }
    }

    if (configDefaultFontSize > 0) {
        cfg.theme.editorFontSize = configDefaultFontSize;
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

        applyThemeStyle();
        applyTypingFormatToCursor();
    }

    std::function<void(bool)> onLeaderModeChanged;
    std::function<void()> onFormattingChanged;
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

    void updateConfig(const ThemeConfig &newTheme, const KeybindingsConfig &newKeys) {
        std::array<QColor, 4> oldPalette = theme_.textColors;
        int oldDefaultSize = theme_.editorFontSize;
        theme_ = newTheme;
        keys_ = newKeys;
        applyThemeStyle();

        if (activeFontSize_ == oldDefaultSize) {
            activeFontSize_ = theme_.editorFontSize;
        }

        // Remap existing styled text spans
        QTextBlock block = document()->begin();
        QTextCursor cursor(document());
        while (block.isValid()) {
            if (!isDividerBlock(block)) {
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    QTextFragment frag = it.fragment();
                    if (!frag.isValid()) continue;
                    QTextCharFormat curFmt = frag.charFormat();
                    QColor fragCol = curFmt.foreground().color();
                    int slot = matchColorSlot(fragCol, oldPalette);

                    QFont curFont = curFmt.font();
                    int px = curFont.pixelSize() > 0 ? curFont.pixelSize() : theme_.editorFontSize;
                    bool b = curFont.weight() >= QFont::Bold;
                    bool itl = curFont.italic();

                    cursor.setPosition(frag.position());
                    cursor.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                    QTextCharFormat fmt;
                    fmt.setForeground(slot >= 0 ? theme_.textColors[slot] : fragCol);
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
        formatExistingDividers();
        applyTypingFormatToCursor();
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

    void adjustTypingFontSize(int deltaPx) {
        activeFontSize_ = std::clamp(activeFontSize_ + deltaPx, 8, 72);
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection()) {
            applySpanFontOverride(cursor, [deltaPx](bool &, bool &, int &px) {
                px = std::clamp(px + deltaPx, 8, 72);
            });
        }
        applyTypingFormatToCursor();
        if (onFormattingChanged) {
            onFormattingChanged();
        }
    }

    void resetFormatting() {
        activeColorIndex_ = 0;
        activeColor_ = theme_.textColors[0];
        activeBold_ = false;
        activeItalic_ = false;
        highlightMode_ = false;
        activeFontSize_ = theme_.editorFontSize;

        QTextCursor cursor = textCursor();
        if (cursor.hasSelection()) {
            QTextCharFormat fmt = currentTypingCharFormat();
            cursor.mergeCharFormat(fmt);
            formatExistingDividers();
        }
        applyTypingFormatToCursor();
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

        // 5. Ensure typed characters always use the currently selected typing color/bold/italic/size
        if (!event->text().isEmpty() && noModifiers) {
            applyTypingFormatToCursor();
        }

        QTextEdit::keyPressEvent(event);

        // 6. Check if the user just finished typing the divider trigger (default "---")
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
                        QFont f = cfmt.font();
                        bool itl = f.italic();
                        bool b = f.weight() >= QFont::Bold;
                        int px = f.pixelSize() > 0 ? f.pixelSize() : activeFontSize_;
                        mutator(itl, b, px);
                        mods.append(SpanMod{oStart, oEnd, col, itl, b, px});
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

    void applyThemeStyle() {
        QString sheet = QString(R"(
            QTextEdit {
                background-color: %1;
                color: %2;
                border: none;
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

        QTextCharFormat divFmt;
        divFmt.setForeground(Qt::transparent);
        cursor.insertText(QStringLiteral("---"), divFmt);

        cursor.insertBlock();
        QTextCharFormat textFmt = currentTypingCharFormat();
        cursor.setBlockCharFormat(textFmt);
        cursor.setCharFormat(textFmt);

        cursor.endEditBlock();
        setTextCursor(cursor);
        applyTypingFormatToCursor();
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
};

// Bottom-left formatting icon button (Bold, Italic, Up Arrow, Down Arrow)
class FormatIconButton : public QWidget {
public:
    enum class IconType {
        Bold,
        Italic,
        ArrowUp,
        ArrowDown
    };

    FormatIconButton(IconType type, const ThemeConfig &theme, QWidget *parent = nullptr)
        : QWidget(parent), type_(type), theme_(theme) {
        setFixedSize(14, 14);
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
        p.setRenderHint(QPainter::TextAntialiasing, true);

        QColor iconColor;
        if (highlighted_) {
            iconColor = theme_.plusHoverColor;
        } else if (active_ || hovered_) {
            iconColor = theme_.activeTabColor;
        } else {
            iconColor = theme_.inactiveTabColor;
        }

        if (type_ == IconType::Bold) {
            QFont f = makeFontWithFallback(theme_.headerFontFamily, 13, QFont::ExtraBold, false);
            p.setFont(f);
            p.setPen(iconColor);
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("B"));
        } else if (type_ == IconType::Italic) {
            QFont f = makeFontWithFallback(theme_.headerFontFamily, 13, QFont::DemiBold, true);
            p.setFont(f);
            p.setPen(iconColor);
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("I"));
        } else if (type_ == IconType::ArrowUp) {
            QPen pen(iconColor, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            // Vertical shaft + up chevron
            p.drawLine(QPointF(7.0, 11.5), QPointF(7.0, 2.8));
            p.drawLine(QPointF(3.5, 6.2), QPointF(7.0, 2.5));
            p.drawLine(QPointF(10.5, 6.2), QPointF(7.0, 2.5));
        } else if (type_ == IconType::ArrowDown) {
            QPen pen(iconColor, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            // Vertical shaft + down chevron
            p.drawLine(QPointF(7.0, 2.5), QPointF(7.0, 11.2));
            p.drawLine(QPointF(3.5, 7.8), QPointF(7.0, 11.5));
            p.drawLine(QPointF(10.5, 7.8), QPointF(7.0, 11.5));
        }
    }

private:
    IconType type_;
    ThemeConfig theme_;
    bool active_ = false;
    bool hovered_ = false;
    bool highlighted_ = false;
};

// 12x12 circular color button matching Figma nodes 1:4, 1:5, 1:7, 1:9
class ColorDotButton : public QWidget {
public:
    ColorDotButton(const QColor &color, const QColor &bgColor, QWidget *parent = nullptr)
        : QWidget(parent), color_(color), bgColor_(bgColor) {
        setFixedSize(12, 12);
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

        if (squareMode_) {
            p.drawRoundedRect(QRectF(0.0, 0.0, 12.0, 12.0), 1.5, 1.5);
        } else {
            p.drawEllipse(QRectF(0.0, 0.0, 12.0, 12.0));
        }

        // Small black circle in the center of the active swatch in both modes
        if (activeSwatch_) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#000000"));
            p.drawEllipse(QRectF(4.0, 4.0, 4.0, 4.0));
        }

        // Subtle inner ring indicator when 'jj' leader mode is waiting for a key
        if (highlighted_) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(bgColor_, 1.6));
            if (squareMode_) {
                p.drawRoundedRect(QRectF(2.0, 2.0, 8.0, 8.0), 1.0, 1.0);
            } else {
                p.drawEllipse(QRectF(2.0, 2.0, 8.0, 8.0));
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

// Custom top header bar matching Figma coordinates
class HeaderTabBar : public QWidget {
public:
    explicit HeaderTabBar(const ThemeConfig &theme, QWidget *parent = nullptr)
        : QWidget(parent), theme_(theme) {
        setFixedHeight(36);
        setMouseTracking(true);

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
        for (int i = 0; i < layouts.size(); ++i) {
            const TabLayout &tl = layouts[i];
            bool isSelected = (i == currentIndex_);
            bool isHovered = (i == hoveredTabIndex_);

            QColor textColor = isSelected
                                   ? theme_.activeTabColor
                                   : (isHovered ? theme_.hoverTabColor
                                                : theme_.inactiveTabColor);
            p.setPen(textColor);
            p.drawText(tl.textRect, Qt::AlignLeft | Qt::AlignVCenter, titles_[i]);

            if (isHovered) {
                QColor closeColor = closeHovered_ ? theme_.activeTabColor : theme_.hoverTabColor;
                p.setPen(closeColor);
                p.drawText(tl.closeRect, Qt::AlignCenter, QStringLiteral("×"));
            }
        }

        QRect plusR = plusButtonRect();
        p.setPen(plusHovered_ ? theme_.plusHoverColor : theme_.plusColor);
        p.drawText(plusR, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("+"));
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

    QRect plusButtonRect() const {
        return QRect(width() - 24, 9, 10, 17);
    }

    QRect plusHitRect() const {
        return plusButtonRect().adjusted(-6, -4, 6, 4);
    }

    QList<TabLayout> computeLayouts() const {
        QList<TabLayout> list;
        QFontMetrics fm(font_);
        int h = std::max(17, fm.height());
        int x = 20;
        int maxRight = width() - 40;
        for (int i = 0; i < titles_.size(); ++i) {
            int textW = std::max(24, fm.horizontalAdvance(titles_[i]) + 2);
            if (x + textW > maxRight && i > 0) {
                break;
            }
            QRect textR(x, 10, textW, h);
            QRect closeR(x + textW + 2, 10, 14, h);
            QRect hitR(x - 2, 6, textW + 18, h + 8);
            list.append(TabLayout{textR, closeR, hitR});
            x += textW + 22;
        }
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
            QLineEdit {
                background-color: %3;
                color: %4;
                border: none;
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
        QStringList files = dir.entryList(QStringList() << "*.txt", QDir::Files, QDir::Name | QDir::Reversed);
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
                    preview = trimmed.left(48);
                    break;
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
        editorLayout->setContentsMargins(20, 6, 20, 12);
        editorLayout->setSpacing(0);

        editorStack_ = new QStackedWidget(editorContainer);
        editorLayout->addWidget(editorStack_);
        mainLayout->addWidget(editorContainer, 1);

        // Bottom bar:
        // - Lower-left (x = 20): 4 formatting icons (Bold, Italic, Up Arrow, Down Arrow)
        // - Lower-right (20px from right): 4 circular color dots (96 x 12)
        auto *bottomBar = new QWidget(centralRoot_);
        bottomBar->setFixedHeight(34);
        auto *bottomLayout = new QHBoxLayout(bottomBar);
        bottomLayout->setContentsMargins(20, 0, 20, 20);
        bottomLayout->setSpacing(0);

        // Lower-left formatting icons row
        auto *formatRow = new QWidget(bottomBar);
        formatRow->setFixedHeight(14);
        auto *formatLayout = new QHBoxLayout(formatRow);
        formatLayout->setContentsMargins(0, 0, 0, 0);
        formatLayout->setSpacing(14);

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
        formatButtons_[2]->setToolTip("Increase font size (jj + d)");
        formatButtons_[2]->onClicked = [this]() {
            if (auto *ed = currentEditor()) {
                ed->adjustTypingFontSize(appConfig_.keys.fontSizeStep);
                ed->setFocus();
            }
        };

        formatButtons_[3] = new FormatIconButton(FormatIconButton::IconType::ArrowDown, appConfig_.theme, formatRow);
        formatButtons_[3]->setToolTip("Decrease font size (jj + f)");
        formatButtons_[3]->onClicked = [this]() {
            if (auto *ed = currentEditor()) {
                ed->adjustTypingFontSize(-appConfig_.keys.fontSizeStep);
                ed->setFocus();
            }
        };

        for (int i = 0; i < 4; ++i) {
            formatLayout->addWidget(formatButtons_[i]);
        }

        // Lower-right 4 color dots (96 x 12)
        auto *colorsRow = new QWidget(bottomBar);
        colorsRow->setFixedSize(96, 12);
        auto *colorsLayout = new QHBoxLayout(colorsRow);
        colorsLayout->setContentsMargins(0, 0, 0, 0);
        colorsLayout->setSpacing(16);

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
    }

    void syncFormatButtonsState() {
        NoteEditor *ed = currentEditor();
        if (formatButtons_[0]) {
            formatButtons_[0]->setActive(ed && ed->isBoldActive());
        }
        if (formatButtons_[1]) {
            formatButtons_[1]->setActive(ed && ed->isItalicActive());
        }
        bool hl = ed && ed->isHighlightMode();
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
        if (!appConfig_.configFilePath.isEmpty() && QFileInfo::exists(appConfig_.configFilePath)) {
            configWatcher_->addPath(appConfig_.configFilePath);
        }
        if (!appConfig_.themeFilePath.isEmpty() && QFileInfo::exists(appConfig_.themeFilePath)) {
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
                int diff = appConfig_.theme.editorFontSize - ed->activeFontSize();
                ed->adjustTypingFontSize(diff);
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

        QDir dir(notesDir_);
        QString candidate = dir.filePath(prefix + base + ".txt");
        if (candidate == ignoreFilePath || !QFileInfo::exists(candidate)) {
            return candidate;
        }

        int suffix = 2;
        while (true) {
            candidate = dir.filePath(QString("%1%2 %3.txt").arg(prefix, base).arg(suffix));
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
        editor->setPlainText(initialContent);
        if (!styleSpans.isEmpty()) {
            editor->importStyleSpans(styleSpans);
        } else {
            editor->formatExistingDividers();
        }
        int initSize = (activeFontSize > 0) ? activeFontSize : appConfig_.theme.editorFontSize;
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
            QJsonArray spans = ed->exportStyleSpans();
            QString activeCol = ed->activeTypingColor().name(QColor::HexRgb).toUpper();
            int activeSlot = ed->activeTypingColorIndex();
            bool b = ed->isBoldActive();
            bool itl = ed->isItalicActive();
            int sz = ed->activeFontSize();

            savedSpansMap_[fname] = spans;
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
            out << ed->toPlainText();
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
                    savedSpansMap_[fname] = ed->exportStyleSpans();
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
        QStringList existingFiles = dir.entryList(QStringList() << "*.txt", QDir::Files, QDir::Name);

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
                resize(std::max(300, w), std::max(240, h));

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
    QApplication::setApplicationName("mimenotes");
    QApplication::setApplicationDisplayName("Mime Notes");
    QApplication::setDesktopFileName("mimenotes");
    QApplication::setApplicationVersion("1.0");

    QIcon appIcon = QIcon::fromTheme("mimenotes");
    if (appIcon.isNull()) {
        QString localIcon = QDir(QCoreApplication::applicationDirPath()).filePath("mimenotes.svg");
        if (QFileInfo::exists(localIcon)) {
            appIcon = QIcon(localIcon);
        } else {
            QString userIcon = QDir::home().filePath(".local/share/icons/hicolor/scalable/apps/mimenotes.svg");
            if (QFileInfo::exists(userIcon)) {
                appIcon = QIcon(userIcon);
            }
        }
    }
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription("Mime Notes — Minimalist plain-text tabbed notepad with automatic saving.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption dirOption(
        QStringList() << "d" << "dir",
        "Directory where note .txt files are stored (default: ~/Documents/Mimenotes or ~/Mimenotes).",
        "path");
    parser.addOption(dirOption);

    QCommandLineOption configOption(
        QStringList() << "c" << "config",
        "Path to config.json (default: ./config.json or ~/.config/mimenotes/config.json).",
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
    window.show();

    QString screenshotPath = parser.value(screenshotOption);
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(50, &app, [&window, screenshotPath, &app]() {
            window.grab().save(screenshotPath);
            app.quit();
        });
    }

    return app.exec();
}
