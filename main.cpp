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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMainWindow>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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

    // 4 bottom-left text colors
    std::array<QColor, 4> textColors = {
        QColor("#FFFFFF"), // Color 1
        QColor("#B4C9AE"), // Color 2
        QColor("#C3B7DB"), // Color 3
        QColor("#A8C9DA")  // Color 4
    };
};

struct KeybindingsConfig {
    QString newTab = QStringLiteral("Ctrl+T");
    QString newTabAlt = QStringLiteral("Ctrl+N");
    QString closeTab = QStringLiteral("Ctrl+W");
    QString reopenClosedTab = QStringLiteral("Ctrl+Shift+T");
    QString save = QStringLiteral("Ctrl+S");
    QString renameTab = QStringLiteral("F2");
    QString renameTabAlt = QStringLiteral("Ctrl+R");
    QString nextTab = QStringLiteral("Ctrl+Tab");
    QString nextTabAlt = QStringLiteral("Ctrl+PgDown");
    QString prevTab = QStringLiteral("Ctrl+Shift+Tab");
    QString prevTabAlt = QStringLiteral("Ctrl+PgUp");
    QString zoomIn = QStringLiteral("Ctrl+=");
    QString zoomOut = QStringLiteral("Ctrl+-");
    QString zoomReset = QStringLiteral("Ctrl+0");

    QString dividerTrigger = QStringLiteral("---");
    QString colorModeTrigger = QStringLiteral("jj");
    qint64 colorModeTimeoutMs = 350;
    std::array<QString, 4> colorKeys = {
        QStringLiteral("j"),
        QStringLiteral("k"),
        QStringLiteral("l"),
        QStringLiteral(";")
    };
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
    QJsonArray colorSpans;
    QString activeColor;
    int activeColorIndex = 0;
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

QFont makeFontWithFallback(const QString &primaryFamily, int pixelSize, QFont::Weight weight) {
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

    // Locate config.json:
    // 1. explicit --config <path> or $NOTES_CONFIG
    // 2. ./config.json in current directory
    // 3. <appBinaryDir>/config.json
    // 4. ~/.config/notes/config.json
    QStringList candidates;
    if (!explicitConfigPath.isEmpty()) {
        candidates << expandTilde(explicitConfigPath);
    }
    QString envCfg = qEnvironmentVariable("NOTES_CONFIG");
    if (!envCfg.isEmpty()) {
        candidates << expandTilde(envCfg);
    }
    candidates << QDir::current().filePath("config.json");
    candidates << QDir(QCoreApplication::applicationDirPath()).filePath("config.json");
    candidates << QDir::home().filePath(".config/notes/config.json");

    for (const QString &cand : candidates) {
        if (QFileInfo::exists(cand)) {
            cfg.configFilePath = QFileInfo(cand).absoluteFilePath();
            break;
        }
    }

    if (!cfg.configFilePath.isEmpty()) {
        cfg.themesDirPath = QFileInfo(cfg.configFilePath).absoluteDir().filePath("themes");
        QFile file(cfg.configFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                cfg.themeName = root["theme"].toString(cfg.themeName);
                cfg.notesDir = expandTilde(root["notes_dir"].toString());

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
                    QString colTrig = kb["color_mode_trigger"].toString(cfg.keys.colorModeTrigger);
                    if (colTrig.size() == 2) {
                        cfg.keys.colorModeTrigger = colTrig;
                    }
                    cfg.keys.colorModeTimeoutMs = std::clamp<qint64>(
                        kb["color_mode_timeout_ms"].toInt(static_cast<int>(cfg.keys.colorModeTimeoutMs)),
                        80, 2000);

                    if (kb["color_keys"].isArray()) {
                        QJsonArray ck = kb["color_keys"].toArray();
                        for (int i = 0; i < std::min<int>(4, ck.size()); ++i) {
                            QString k = ck[i].toString();
                            if (!k.isEmpty()) {
                                cfg.keys.colorKeys[i] = k;
                            }
                        }
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
    themeCandidates << QDir::home().filePath(".config/notes/themes/" + themeNameClean + ".json");

    for (const QString &tc : themeCandidates) {
        if (QFileInfo::exists(tc)) {
            cfg.themeFilePath = QFileInfo(tc).absoluteFilePath();
            cfg.theme = loadThemeFromFile(cfg.themeFilePath);
            break;
        }
    }

    return cfg;
}

// Plain-text editor supporting:
// - per-character colors from the 4 theme colors
// - configurable quick chord (default 'jj') followed by 4 color keys (default 'j', 'k', 'l', ';')
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
          activeColorIndex_(0) {
        setFrameStyle(QFrame::NoFrame);
        setAcceptRichText(false);
        document()->setDocumentMargin(0);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        applyThemeStyle();
        applyTypingColorToCursor();
    }

    std::function<void(bool)> onColorModeChanged;
    std::function<void(int, const QColor &)> onColorPicked;

    QColor activeTypingColor() const { return activeColor_; }
    int activeTypingColorIndex() const { return activeColorIndex_; }
    bool isColorModeActive() const { return colorMode_; }

    void updateConfig(const ThemeConfig &newTheme, const KeybindingsConfig &newKeys) {
        std::array<QColor, 4> oldPalette = theme_.textColors;
        theme_ = newTheme;
        keys_ = newKeys;
        applyThemeStyle();

        // Remap existing colored text spans that matched one of the 4 theme palette slots
        QTextBlock block = document()->begin();
        QTextCursor cursor(document());
        while (block.isValid()) {
            if (!isDividerBlock(block)) {
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    QTextFragment frag = it.fragment();
                    if (!frag.isValid()) continue;
                    QColor fragCol = frag.charFormat().foreground().color();
                    int slot = matchColorSlot(fragCol, oldPalette);
                    if (slot >= 0) {
                        cursor.setPosition(frag.position());
                        cursor.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                        QTextCharFormat fmt;
                        fmt.setForeground(theme_.textColors[slot]);
                        cursor.mergeCharFormat(fmt);
                    }
                }
            }
            block = block.next();
        }

        if (activeColorIndex_ >= 0 && activeColorIndex_ < 4) {
            activeColor_ = theme_.textColors[activeColorIndex_];
        }
        formatExistingDividers();
        applyTypingColorToCursor();
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
        QTextCharFormat fmt;
        fmt.setForeground(activeColor_);
        if (applyToSelection && cursor.hasSelection()) {
            cursor.mergeCharFormat(fmt);
            formatExistingDividers();
        }
        mergeCurrentCharFormat(fmt);
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

    QJsonArray exportColorSpans() const {
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
                QColor col = frag.charFormat().foreground().color();
                if (!col.isValid() || col.alpha() == 0) {
                    col = theme_.textColors[0];
                }
                int slot = matchColorSlot(col, theme_.textColors);
                QJsonObject obj;
                obj["pos"] = frag.position();
                obj["len"] = frag.length();
                obj["color"] = col.name(QColor::HexRgb).toUpper();
                if (slot >= 0) {
                    obj["slot"] = slot;
                }
                spans.append(obj);
            }
            block = block.next();
        }
        return spans;
    }

    void importColorSpans(const QJsonArray &spans) {
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
            cursor.setPosition(pos);
            cursor.setPosition(end, QTextCursor::KeepAnchor);
            QTextCharFormat fmt;
            fmt.setForeground(col);
            cursor.mergeCharFormat(fmt);
        }
        formatExistingDividers();
        applyTypingColorToCursor();
    }

protected:
    void focusOutEvent(QFocusEvent *event) override {
        setColorMode(false);
        lastTriggerFirstPressMs_ = 0;
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
        // 1. Handle active color mode (entered via quick color_mode_trigger chord, e.g. "jj")
        if (colorMode_) {
            int key = event->key();
            if (key == Qt::Key_Shift || key == Qt::Key_Control ||
                key == Qt::Key_Alt || key == Qt::Key_Meta) {
                QTextEdit::keyPressEvent(event);
                return;
            }

            QString txt = event->text();
            int colorIndex = -1;
            for (int i = 0; i < 4; ++i) {
                if (!txt.isEmpty() && txt.compare(keys_.colorKeys[i], Qt::CaseInsensitive) == 0) {
                    colorIndex = i;
                    break;
                }
            }

            setColorMode(false);
            lastTriggerFirstPressMs_ = 0;

            if (colorIndex >= 0) {
                setActiveTypingColorSlot(colorIndex, true);
                if (onColorPicked) {
                    onColorPicked(colorIndex, theme_.textColors[colorIndex]);
                }
                event->accept();
                return;
            }

            if (key == Qt::Key_Escape) {
                event->accept();
                return;
            }
        }

        // 2. Detect quick 2-character color_mode_trigger chord (default "jj")
        bool noModifiers = !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        QString trig = keys_.colorModeTrigger;
        if (noModifiers && trig.size() == 2 && !event->text().isEmpty()) {
            QString firstChar = trig.left(1);
            QString secondChar = trig.mid(1, 1);
            qint64 now = QDateTime::currentMSecsSinceEpoch();

            if (event->text() == secondChar &&
                lastTriggerFirstPressMs_ > 0 &&
                (now - lastTriggerFirstPressMs_) <= keys_.colorModeTimeoutMs) {
                QTextCursor c = textCursor();
                int posInBlk = c.positionInBlock();
                if (!c.hasSelection() && posInBlk > 0 &&
                    c.block().text().mid(posInBlk - 1, 1) == firstChar) {
                    c.deletePreviousChar();
                    setTextCursor(c);
                    lastTriggerFirstPressMs_ = 0;
                    setColorMode(true);
                    event->accept();
                    return;
                }
            }

            if (event->text() == firstChar) {
                lastTriggerFirstPressMs_ = now;
            } else {
                lastTriggerFirstPressMs_ = 0;
            }
        } else {
            lastTriggerFirstPressMs_ = 0;
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
                QTextCharFormat fmt;
                fmt.setForeground(activeColor_);
                cursor.setCharFormat(fmt);
                cursor.endEditBlock();
                setTextCursor(cursor);
                applyTypingColorToCursor();
                event->accept();
                return;
            }
            if (!event->text().isEmpty() && noModifiers) {
                cursor.beginEditBlock();
                cursor.movePosition(QTextCursor::EndOfBlock);
                cursor.insertBlock();
                QTextCharFormat fmt;
                fmt.setForeground(activeColor_);
                cursor.setCharFormat(fmt);
                cursor.endEditBlock();
                setTextCursor(cursor);
            }
        }

        // 5. Ensure typed characters always use the currently selected typing color
        if (!event->text().isEmpty() && noModifiers) {
            applyTypingColorToCursor();
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
        QTextCharFormat fmt;
        fmt.setForeground(activeColor_);
        cursor.insertText(source->text(), fmt);
        setTextCursor(cursor);
        formatExistingDividers();
    }

private:
    static int matchColorSlot(const QColor &c, const std::array<QColor, 4> &palette) {
        if (!c.isValid()) return -1;
        for (int i = 0; i < 4; ++i) {
            if (c.rgb() == palette[i].rgb()) {
                return i;
            }
        }
        return -1;
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

    void setColorMode(bool active) {
        if (colorMode_ != active) {
            colorMode_ = active;
            if (onColorModeChanged) {
                onColorModeChanged(colorMode_);
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
        applyTypingColorToCursor();
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
        QTextCharFormat textFmt;
        textFmt.setForeground(activeColor_);
        cursor.setBlockCharFormat(textFmt);
        cursor.setCharFormat(textFmt);

        cursor.endEditBlock();
        setTextCursor(cursor);
        applyTypingColorToCursor();
        viewport()->update();
    }

    void applyTypingColorToCursor() {
        QTextCharFormat fmt;
        fmt.setForeground(activeColor_);
        mergeCurrentCharFormat(fmt);
    }

    ThemeConfig theme_;
    KeybindingsConfig keys_;
    QColor activeColor_;
    int activeColorIndex_ = 0;
    qint64 lastTriggerFirstPressMs_ = 0;
    bool colorMode_ = false;
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

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(color_);
        p.drawEllipse(QRectF(0.0, 0.0, 12.0, 12.0));

        // Subtle inner ring indicator when 'jj' color mode is waiting for j/k/l/;
        if (highlighted_) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(bgColor_, 2.0));
            p.drawEllipse(QRectF(2.5, 2.5, 7.0, 7.0));
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

class NotepadWindow : public QMainWindow {
public:
    NotepadWindow(const AppConfig &appConfig,
                  const QString &explicitNotesDir,
                  const QString &explicitConfigPath,
                  const QString &themeOverride)
        : appConfig_(appConfig),
          explicitConfigPath_(explicitConfigPath),
          themeOverride_(themeOverride) {
        setWindowTitle("Notes");

        if (!explicitNotesDir.isEmpty()) {
            notesDir_ = QDir::cleanPath(expandTilde(explicitNotesDir));
        } else if (!appConfig_.notesDir.isEmpty()) {
            notesDir_ = QDir::cleanPath(appConfig_.notesDir);
        } else {
            notesDir_ = QDir::home().filePath("Notes");
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

        // Bottom bar containing node 1:13 ("colors"):
        // x = 20, y = 445 in 503x477 (20px from left, 20px from bottom), width = 96, height = 12
        auto *bottomBar = new QWidget(centralRoot_);
        bottomBar->setFixedHeight(32);
        auto *bottomLayout = new QHBoxLayout(bottomBar);
        bottomLayout->setContentsMargins(20, 0, 20, 20);
        bottomLayout->setSpacing(0);

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

        bottomLayout->addWidget(colorsRow, 0, Qt::AlignLeft | Qt::AlignTop);
        bottomLayout->addStretch(1);
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

        setupShortcuts();
        loadSession();
        setupConfigWatcher();
    }

protected:
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
        for (int i = 0; i < 4; ++i) {
            if (colorButtons_[i]) {
                colorButtons_[i]->setColors(appConfig_.theme.textColors[i], appConfig_.theme.background);
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

        bind(kb.zoomIn, [this]() { adjustZoom(1); });
        bind(kb.zoomOut, [this]() { adjustZoom(-1); });
        bind(kb.zoomReset, [this]() { resetZoom(); });
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
        editorStack_->setCurrentIndex(index);
        headerBar_->setCurrentIndex(index);
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
            titles.append(QFileInfo(filePathAt(i)).completeBaseName());
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

    void adjustZoom(int delta) {
        fontPixelSize_ = std::clamp(fontPixelSize_ + delta, 10, 48);
        applyFontToEditors();
    }

    void resetZoom() {
        fontPixelSize_ = appConfig_.theme.editorFontSize;
        applyFontToEditors();
    }

    void applyFontToEditors() {
        rebuildEditorFont();
        for (int i = 0; i < tabCount(); ++i) {
            if (auto *ed = editorAt(i)) {
                ed->setFont(editorFont_);
                QFontMetrics fm(editorFont_);
                ed->setTabStopDistance(fm.horizontalAdvance(' ') * 4);
            }
        }
        saveSessionState();
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

    QString uniqueNoteFilePath(const QString &desiredBaseName, const QString &ignoreFilePath = {}) const {
        QString base = sanitizeTitle(desiredBaseName);
        if (base.isEmpty()) {
            base = "Note";
        }

        QDir dir(notesDir_);
        QString candidate = dir.filePath(base + ".txt");
        if (candidate == ignoreFilePath || !QFileInfo::exists(candidate)) {
            return candidate;
        }

        int suffix = 2;
        while (true) {
            candidate = dir.filePath(QString("%1 %2.txt").arg(base).arg(suffix));
            if (candidate == ignoreFilePath || !QFileInfo::exists(candidate)) {
                return candidate;
            }
            ++suffix;
        }
    }

    QString nextDefaultNoteFilePath() const {
        QDir dir(notesDir_);
        int n = 1;
        while (true) {
            QString candidate = dir.filePath(QString("Note %1.txt").arg(n));
            if (!QFileInfo::exists(candidate)) {
                return candidate;
            }
            ++n;
        }
    }

    int addTabForFile(const QString &filePath,
                      const QString &initialContent,
                      bool writeImmediately,
                      int insertAt = -1,
                      const QJsonArray &colorSpans = {},
                      int activeColorSlot = 0,
                      const QColor &activeColorFallback = {}) {
        auto *editor = new NoteEditor(appConfig_.theme, appConfig_.keys, editorStack_);
        editor->setFont(editorFont_);
        QFontMetrics fm(editorFont_);
        editor->setTabStopDistance(fm.horizontalAdvance(' ') * 4);
        editor->setPlainText(initialContent);
        if (!colorSpans.isEmpty()) {
            editor->importColorSpans(colorSpans);
        } else {
            editor->formatExistingDividers();
        }
        if (activeColorSlot >= 0 && activeColorSlot < 4) {
            editor->setActiveTypingColorSlot(activeColorSlot, false);
        } else if (activeColorFallback.isValid()) {
            editor->setActiveTypingColor(activeColorFallback, false);
        } else {
            editor->setActiveTypingColorSlot(0, false);
        }
        editor->setProperty("noteFilePath", filePath);
        editor->setProperty("noteDirty", writeImmediately);

        editor->onColorModeChanged = [this](bool active) {
            setColorDotsHighlighted(active);
        };
        editor->onColorPicked = [this, editor](int, const QColor &) {
            setDirty(editor, true);
            autoSaveTimer_->start();
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
        QString oldTitle = QFileInfo(oldPath).completeBaseName();
        if (clean == oldTitle) {
            return;
        }

        QString newPath = uniqueNoteFilePath(clean, oldPath);
        if (QFile::exists(oldPath)) {
            if (!QFile::rename(oldPath, newPath)) {
                return;
            }
        } else {
            setFilePathAt(index, newPath);
            saveEditor(editorAt(index));
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
        QString title = QFileInfo(path).completeBaseName();
        QString content = ed ? ed->toPlainText() : QString();

        if (content.trimmed().isEmpty()) {
            QFile::remove(path);
        } else {
            QDir trashDir(trashDirPath());
            if (!trashDir.exists()) {
                trashDir.mkpath(".");
            }
            QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
            QString trashFile = trashDir.filePath(QString("%1_%2.txt").arg(stamp, title));
            if (QFile::rename(path, trashFile)) {
                closedStack_.push(ClosedNote{
                    title,
                    trashFile,
                    ed ? ed->exportColorSpans() : QJsonArray{},
                    ed ? ed->activeTypingColor().name(QColor::HexRgb).toUpper() : QStringLiteral("#FFFFFF"),
                    ed ? ed->activeTypingColorIndex() : 0,
                    index});
            }
        }

        QWidget *w = editorStack_->widget(index);
        editorStack_->removeWidget(w);
        delete w;

        if (tabCount() == 0) {
            createNewTab();
        } else {
            syncHeaderTabs();
            saveSessionState();
        }
    }

    void reopenClosedTab() {
        while (!closedStack_.isEmpty()) {
            ClosedNote item = closedStack_.pop();
            if (!QFile::exists(item.trashFilePath)) {
                continue;
            }
            QString restoredPath = uniqueNoteFilePath(item.title);
            if (!QFile::rename(item.trashFilePath, restoredPath)) {
                continue;
            }
            QString content = readFileContent(restoredPath);
            int targetIdx = std::clamp(item.index, 0, tabCount());
            int idx = addTabForFile(
                restoredPath, content, false, targetIdx, item.colorSpans, item.activeColorIndex, QColor(item.activeColor));
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
        QJsonObject colorsMap;
        QJsonObject activeColorsMap;
        QJsonObject activeSlotsMap;
        for (int i = 0; i < tabCount(); ++i) {
            QString path = filePathAt(i);
            if (!path.isEmpty()) {
                QString fname = QFileInfo(path).fileName();
                tabsArray.append(fname);
                if (auto *ed = editorAt(i)) {
                    colorsMap[fname] = ed->exportColorSpans();
                    activeColorsMap[fname] = ed->activeTypingColor().name(QColor::HexRgb).toUpper();
                    activeSlotsMap[fname] = ed->activeTypingColorIndex();
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
        root["tab_color_spans"] = colorsMap;
        root["tab_active_colors"] = activeColorsMap;
        root["tab_active_color_slots"] = activeSlotsMap;

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
        QJsonObject colorsMap;
        QJsonObject activeColorsMap;
        QJsonObject activeSlotsMap;

        QFile sessionFile(sessionFilePath());
        if (sessionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument doc = QJsonDocument::fromJson(sessionFile.readAll());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                int w = obj["window_width"].toInt(appConfig_.theme.windowWidth);
                int h = obj["window_height"].toInt(appConfig_.theme.windowHeight);
                resize(std::max(300, w), std::max(240, h));

                activeFileName = obj["active_tab"].toString();
                colorsMap = obj["tab_color_spans"].toObject();
                activeColorsMap = obj["tab_active_colors"].toObject();
                activeSlotsMap = obj["tab_active_color_slots"].toObject();

                QJsonArray arr = obj["open_tabs"].toArray();
                for (const QJsonValue &v : arr) {
                    QString fname = v.toString();
                    if (existingFiles.contains(fname) && !orderedFiles.contains(fname)) {
                        orderedFiles.append(fname);
                    }
                }
            }
        }

        for (const QString &fname : existingFiles) {
            if (!orderedFiles.contains(fname)) {
                orderedFiles.append(fname);
            }
        }

        int activeIndex = 0;
        for (const QString &fname : orderedFiles) {
            QString fullPath = dir.filePath(fname);
            QString content = readFileContent(fullPath);
            QJsonArray spans = colorsMap[fname].toArray();
            int slot = activeSlotsMap.contains(fname) ? activeSlotsMap[fname].toInt(0) : -1;
            QColor activeCol(activeColorsMap[fname].toString());
            int idx = addTabForFile(fullPath, content, false, -1, spans, slot, activeCol);
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
        saveSessionState();
    }

    AppConfig appConfig_;
    QString explicitConfigPath_;
    QString themeOverride_;
    QString notesDir_;
    QWidget *centralRoot_ = nullptr;
    HeaderTabBar *headerBar_ = nullptr;
    QStackedWidget *editorStack_ = nullptr;
    std::array<ColorDotButton *, 4> colorButtons_ = {nullptr, nullptr, nullptr, nullptr};
    QList<QShortcut *> shortcuts_;
    QFileSystemWatcher *configWatcher_ = nullptr;
    QTimer *autoSaveTimer_ = nullptr;
    QFont editorFont_;
    int fontPixelSize_ = 16;
    bool loadingSession_ = false;
    QStack<ClosedNote> closedStack_;
};

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("notes");
    QApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Minimalist plain-text tabbed notepad with automatic saving.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption dirOption(
        QStringList() << "d" << "dir",
        "Directory where note .txt files are stored (default: from config.json, $NOTES_DIR, or ~/Notes).",
        "path");
    parser.addOption(dirOption);

    QCommandLineOption configOption(
        QStringList() << "c" << "config",
        "Path to config.json (default: ./config.json or ~/.config/notes/config.json).",
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
