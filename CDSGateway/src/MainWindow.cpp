#include "MainWindow.hpp"
#include <QGuiApplication>
#include <QApplication>
#include <QListWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>
#include <QWindow>
#include <QScreen>
#include "PolicyPage.hpp"
#include "LoggingPage.hpp"

CdsGuardMainWindow::CdsGuardMainWindow(QWidget *parent) 
: QMainWindow(parent)
{

    qreal screen_dpi = QGuiApplication::primaryScreen()->logicalDotsPerInch();
    qreal scale_by_96_dpi = screen_dpi / 96.0;  // 96 DPI를 기준 스케일로 잡음

    // 전체 중앙 위젯
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 메인 영역
    main_area_ = new QStackedWidget(this);

    policy_page_ = new PolicyPage(this);
    log_page_ = new LoggingPage(this);

    // 스택에 페이지 등록
    main_area_->addWidget(policy_page_);
    main_area_->addWidget(log_page_);

    // 왼쪽 사이드바
    sidebar_ = new QListWidget(this);
    sidebar_->addItem("정책");
    sidebar_->addItem("로그");
    connect(sidebar_, &QListWidget::currentRowChanged, main_area_, &QStackedWidget::setCurrentIndex);

    // 수평 레이아웃
    QHBoxLayout* layout = new QHBoxLayout;
    layout->addWidget(sidebar_, 1);    // 사이드바 너비 비율 1
    layout->addWidget(main_area_, 4);   // 메인 영역 비율 4

    centralWidget->setLayout(layout);
    
    setWindowTitle("CDS Guard 정책 설정 인터페이스");

    QSize screen_size = QGuiApplication::primaryScreen()->availableGeometry().size();

    resize(screen_size.width() / 2, screen_size.height() / 2);  // 초기 창 크기
    main_area_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    resizeFontByCurrentScreen();
    connect(windowHandle(), &QWindow::screenChanged, this, [this](QScreen*){ resizeFontByCurrentScreen(); });
}

CdsGuardMainWindow::~CdsGuardMainWindow() = default;

void CdsGuardMainWindow::resizeFontByCurrentScreen()
{
    QScreen* screen = this->screen();
    if (!screen) return;

    qreal dpi = screen->logicalDotsPerInch();
    qreal scale = dpi / 96.0;

    QFont font = this->font();
    font.setPointSizeF(12 * scale);
    this->setFont(font);
}

void CdsGuardMainWindow::showEvent(QShowEvent* event)
{
    QScreen* screen = this->screen();  // 현재 창이 표시될 모니터
    QRect screen_geometry = screen->availableGeometry();

    int x = (screen_geometry.width() - width()) / 2;
    int y = (screen_geometry.height() - height()) / 2;

    move(screen_geometry.topLeft() + QPoint(x, y));

    QMainWindow::showEvent(event);  // 반드시 부모 클래스도 호출!
}