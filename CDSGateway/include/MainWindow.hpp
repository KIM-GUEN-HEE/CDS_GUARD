#pragma once

#include <QMainWindow>
#include <QWindow>
#include <QListWidget>
#include <QWidget>
#include <QShowEvent>
#include <QStackedWidget>
#include <QPushButton>
#include <QLineEdit>

class CdsGuardMainWindow : public QMainWindow 
{
    Q_OBJECT
public:
    CdsGuardMainWindow(QWidget *parent = nullptr);
    ~CdsGuardMainWindow();
private slots:
private:
    QListWidget* sidebar_;  // 왼쪽 사이드바 (예: 리스트 형식 메뉴)
    QStackedWidget* main_area_;     // 오른쪽 메인 영역
    QWidget* overview_page_;
    QWidget* policy_page_;
    QWidget* log_page_;
    QWidget* system_page_;
    void resizeFontByCurrentScreen();
    void showEvent(QShowEvent* event);
};