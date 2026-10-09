// ============================================================================
//  findinpage.h — Chrome-style "Find in Page" bar.
//
//  · Ctrl+F       → open
//  · Esc          → close
//  · Enter / F3   → next match
//  · Shift+Enter  → previous match
//  · Live match count: "3 / 47"
// ============================================================================
#pragma once

#include <QWidget>
#include <QString>
#include <functional>

class QLineEdit;
class QLabel;
class QAbstractButton;
class QTimer;
class QWebEngineView;
class QWebEnginePage;

// ============================================================================
//  FindBar
// ============================================================================
class FindBar : public QWidget {
public:
    explicit FindBar(QWidget* parent = nullptr);
    ~FindBar() override;

    void attach(QWebEngineView* view);
    void open();
    void close();
    void findNext();
    void findPrevious();
    bool isOpen() const;

    std::function<void()> onClose;

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

private:
    void doFind(const QString& text, bool forward, bool isNewSearch);
    void updateMatchLabel(int current, int total);
    void clearHighlights();

    QLineEdit*       edit_    = nullptr;
    QLabel*          match_   = nullptr;
    QAbstractButton* prev_    = nullptr;
    QAbstractButton* next_    = nullptr;
    QAbstractButton* close_   = nullptr;

    QWebEngineView*  view_    = nullptr;

    int      current_ = 0;
    int      total_   = 0;
    QString  lastQuery_;
    bool     caseSensitive_ = false;
};
