// ============================================================================
//  commandpalette.h — Ctrl+K command palette (VSCode style).
// ============================================================================
#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include <functional>

class QLineEdit;
class QVariantAnimation;

class CommandPalette : public QWidget {
public:
    explicit CommandPalette(QWidget* parent = nullptr);
    ~CommandPalette() override;

    // Register a command
    void add(const QString& title,
             const QString& shortcut,
             const QString& category,
             const QString& iconName,
             std::function<void()> action);

    // Open / close
    void open();
    void close();

    bool isOpen() const { return isVisible(); }

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    struct Command {
        QString title;
        QString shortcut;
        QString category;
        QString iconName;
        std::function<void()> action;
    };

    void buildUi();
    void rebuildList();
    void select(int index);
    void activate(int index);
    bool matches(const Command& c, const QString& q) const;

    QVector<Command> commands_;
    QVector<int>     visibleIndices_;
    int selectedIndex_ = 0;

    QWidget* card_ = nullptr;
    QLineEdit* search_ = nullptr;
    QWidget* list_ = nullptr;

    QVariantAnimation* anim_ = nullptr;
    qreal opacity_ = 1.0;
};
