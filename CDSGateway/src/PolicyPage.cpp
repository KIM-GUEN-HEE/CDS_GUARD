#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QFileDialog>
#include <QFile>
#include <QComboBox>
#include <QTextStream>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QEventLoop>
#include <QMenu>
#include <QInputDialog>
#include <QDesktopServices>
#include <unordered_map>
#include <fstream>
#include <stack>
#include <tuple>
#include <thread>
#include <mutex>
#include <expected>
#include <QNetworkRequest>
#include <QNetworkReply>
#include "ParseZip.hpp"
#include "PolicyPage.hpp"
#include "DocumentLabelingDialog.hpp"

PolicyPage::PolicyPage(QWidget* _parent) 
: QWidget(_parent)
{
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    QTabWidget* tab_widget = new QTabWidget(this);
    main_layout->addWidget(tab_widget);



    // 금칙어 탭
    QWidget* prohibited_setting_page = new QWidget(this);
    tab_widget->addTab(prohibited_setting_page, "금칙어");

    QVBoxLayout* prohibited_setting_layout = new QVBoxLayout(prohibited_setting_page);



    // 기타 button 추가
    int padding_v = get_dpi_scaled(4);
    int padding_h = get_dpi_scaled(12);
    int radius = get_dpi_scaled(3);
    int font_size = get_dpi_scaled(12);

    QString default_soft_button_style = QString(R"(
        QPushButton 
        {
            background-color: #f0f2f5;
            border: 1px solid #d0d5dd;
            border-radius: %1px;
            padding: %2px %3px;
            color: #2c2c2c;
            font-size: %4px;
        }
    
        QPushButton:hover {
            background-color: #e4e6eb;
        }
    
        QPushButton:pressed {
            background-color: #d8dadf;
        }
    
        QPushButton:disabled {
            color: #999;
            background-color: #f5f5f5;
        }
    )").arg(radius).arg(padding_v).arg(padding_h).arg(font_size);
    
    QHBoxLayout* header_layout = new QHBoxLayout;
    prohibited_setting_layout->addLayout(header_layout);
    add_button_ = new QPushButton("추가", prohibited_setting_page);
    add_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    remove_button_ = new QPushButton("삭제", prohibited_setting_page);
    remove_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    edit_button_ = new QPushButton("수정", prohibited_setting_page);
    edit_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    undo_button_ = new QPushButton("되돌리기", prohibited_setting_page);
    undo_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    build_button_ = new QPushButton("빌드/저장", prohibited_setting_page), build_button_->setEnabled(false);
    build_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    load_file_button_ = new QPushButton("불러오기", prohibited_setting_page);
    load_file_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    for (QPushButton* btn : { add_button_, remove_button_, edit_button_, build_button_, undo_button_, load_file_button_ }) 
    {
        btn->setStyleSheet(default_soft_button_style);
    }
    header_layout->addWidget(add_button_);
    header_layout->addWidget(remove_button_);
    header_layout->addWidget(edit_button_);
    header_layout->addWidget(build_button_);
    header_layout->addWidget(undo_button_);
    header_layout->addWidget(load_file_button_);
    header_layout->addStretch();
    connect(add_button_, &QPushButton::clicked, this, &PolicyPage::add_button_clicked);
    connect(remove_button_, &QPushButton::clicked, this, &PolicyPage::prohibited_word_remove_button_clicked);
    connect(edit_button_, &QPushButton::clicked, this, &PolicyPage::prohibited_word_edit_button_clicked);
    connect(undo_button_, &QPushButton::clicked, this, &PolicyPage::undo_button_clicked);
    connect(build_button_, &QPushButton::clicked, this, &PolicyPage::build_button_clicked);
    connect(load_file_button_, &QPushButton::clicked, this, &PolicyPage::load_button_clicked);

    search_input_ = new QLineEdit(this);
    header_layout->addWidget(search_input_, 0, Qt::AlignRight);
    QAction* search_icon_action = new QAction(search_input_);
    search_icon_action->setIcon(QIcon(":/icons/search-normal.svg"));
    search_icon_action->setObjectName("search_icon");
    search_input_->addAction(search_icon_action, QLineEdit::LeadingPosition);
    search_input_->setPlaceholderText("검색");
    search_input_->setStyleSheet(QString(R"(
        QLineEdit 
        {
            background-color: white;
            border: %1px solid #d0d5dd;
            border-radius: %2px;
            padding: %3px %4px;
            color: #333;
        }
        QLineEdit::focus 
        {
            border: %1px solid #4a66f1;
        }
    )").arg(get_dpi_scaled(1)).arg(get_dpi_scaled(3)).arg(get_dpi_scaled(4)).arg(get_dpi_scaled(1)));
    connect(search_input_, &QLineEdit::returnPressed, this, &PolicyPage::search_button_clicked);
    connect(search_icon_action, &QAction::triggered, this, &PolicyPage::search_button_clicked);

    // list 추가
    QVBoxLayout* prohibited_word_list_layout = new QVBoxLayout;
    prohibited_word_list_layout->setSpacing(0);
    prohibited_setting_layout->addLayout(prohibited_word_list_layout);

    QLabel* prohibited_word_list_header_label = new QLabel("금칙어", prohibited_setting_page);
    int prohibited_list_bottom_px = get_dpi_scaled(1);
    int prohibited_list_padding_size = get_dpi_scaled(8);
    prohibited_word_list_header_label->setStyleSheet(QString(R"(
        QLabel 
        {
            font-weight: bold;
            color: #4a66f1;
            padding: %1px %1px;
            background-color: white;
            border-top: 1px solid #ddd;
            border-bottom: %2px solid #ddd;
        }
    )").arg(prohibited_list_padding_size).arg(prohibited_list_bottom_px));
    prohibited_word_list_layout->addWidget(prohibited_word_list_header_label);

    prohibited_word_list_widget_ = new QListWidget(prohibited_setting_page), prohibited_word_list_widget_->setContextMenuPolicy(Qt::CustomContextMenu);
    prohibited_word_list_widget_->setStyleSheet(QString(R"(
        QListWidget 
        {
            border-left: none;
            border-right: none;
            border-top: none;
            border-bottom: 1px solid #e0e0e0;
        }
        QListWidget::item 
        {
            border-bottom: %1px solid #dddddd;
            padding: %2px;
        }

        QListWidget::item:selected 
        {
            background-color: #4a90e2;
            color: white;
            border-bottom: %1px solid #dddddd;
        }

        QListWidget::item:hover 
        {
            background-color: #f5f7fa;
            color: #000;
        }
    )").arg(prohibited_list_bottom_px).arg(prohibited_list_padding_size));
    prohibited_word_list_layout->addWidget(prohibited_word_list_widget_);
    connect(prohibited_word_list_widget_, &QListWidget::customContextMenuRequested, this, &PolicyPage::prohibit_word_list_right_clicked);
    connect(prohibited_word_list_widget_, &QListWidget::itemChanged, this, &PolicyPage::edit_list_item);

    // list page control 영역
    QHBoxLayout* list_bottom_layout = new QHBoxLayout;
    prohibited_setting_layout->addLayout(list_bottom_layout);

    // 페이지 컨트롤 (좌측 정렬)
    page_control_widget_ = new QWidget(prohibited_setting_page);
    page_control_widget_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    list_bottom_layout->addWidget(page_control_widget_, 0, Qt::AlignLeft);

    // list_bottom_layout->addStretch();
    // list_bottom_layout->addWidget(search_container, 0, Qt::AlignRight);

    // QHBoxLayout* item_num_info_and_setting_layout = new QHBoxLayout(this);
    QWidget* item_num_info_and_setting_widget = new QWidget(prohibited_setting_page);

    QHBoxLayout* item_num_info_and_setting_layout = new QHBoxLayout(item_num_info_and_setting_widget);
    item_num_info_and_setting_layout->setContentsMargins(0, 0, 0, 0);
    item_num_info_and_setting_layout->setSpacing(get_dpi_scaled(8));

    QLabel* page_count_num_label = new QLabel("페이지당 금칙어 수", item_num_info_and_setting_widget);
    page_count_num_label->setStyleSheet("color: #666;");
    item_num_info_and_setting_layout->addWidget(page_count_num_label);

    QComboBox* item_count_selector = new QComboBox(this);
    item_count_selector->addItems({"30", "50", "100", "500", "1000"});
    item_count_selector->setCurrentText("50");
    item_count_selector->setMaximumWidth(get_dpi_scaled(80));
    item_count_selector->setStyleSheet(QString(R"(
            QComboBox 
            {
                background-color: white;
                border: %1px solid #d0d5dd;
                border-radius: %2px;
                padding: %3px %4px;
                font-size: %5px;
                color: #333;
            }
        
            QComboBox::drop-down 
            {
                width: %6px;
                subcontrol-origin: padding;
                subcontrol-position: center right;
                border-left: %1px solid #d0d5dd;
                background: transparent;
            }
        
            QComboBox::down-arrow 
            {
                image: url(:/icons/caret-down.svg);
                width: %7px;
                height: %7px;
            }
        )")
        .arg(get_dpi_scaled(1))   // border (and reused for border-left)
        .arg(get_dpi_scaled(4))   // border-radius
        .arg(get_dpi_scaled(2))   // padding top/bottom
        .arg(get_dpi_scaled(8))  // padding left/right
        .arg(get_dpi_scaled(13))  // font-size
        .arg(get_dpi_scaled(14))  // drop-down width
        .arg(get_dpi_scaled(12))  // down-arrow width & height
    );
    connect(item_count_selector, &QComboBox::currentTextChanged, this, [this, item_count_selector](const QString& text) 
    {
        bool ok = false;
        int count = text.toInt(&ok);
        if (ok) {
            prohibited_list_items_per_page_ = count;
            prohibited_list_current_page_ = 0;
            refresh_prohibited_word_list_widget();
        }
    });
    item_num_info_and_setting_layout->addWidget(item_count_selector);

    total_count_label_ = new QLabel("데이터 없음", item_num_info_and_setting_widget);
    total_count_label_->setStyleSheet("color: #444;");
    item_num_info_and_setting_layout->addWidget(total_count_label_);
    list_bottom_layout->addWidget(item_num_info_and_setting_widget, 0, Qt::AlignRight);


    // 이미지 탭
    QWidget* image_setting_tab = new QWidget(this);
    tab_widget->addTab(image_setting_tab, "이미지");
    QVBoxLayout* image_tab_layout = new QVBoxLayout(image_setting_tab);
    image_setting_tab->setLayout(image_tab_layout);

    QHBoxLayout* image_tab_header_layout = new QHBoxLayout;
    image_tab_layout->addLayout(image_tab_header_layout);
    QLabel* image_label = new QLabel("검열 이미지 셋 업데이트");
    image_label->setStyleSheet(QString(R"(
        QLabel 
        {
            color: #4a66f1;
            font-size: %1px;
            padding: %2px %3px;
            background-color: transparent;
            border-bottom: %4px dashed #ddd;
        }
    )").arg(get_dpi_scaled(16)).arg(get_dpi_scaled(6)).arg(get_dpi_scaled(12)).arg(get_dpi_scaled(1)));
    image_tab_header_layout->addWidget(image_label);
    image_tab_header_layout->addStretch();

    QHBoxLayout* image_tab_text_guide_layout = new QHBoxLayout;
    image_tab_layout->addLayout(image_tab_text_guide_layout);
    QLabel* image_text_guide_label = new QLabel;
    QString image_text_guide_string = "검열할 이미지를 '검열 이미지 폴더'에 넣고, '검열 이미지 셋 업데이트' 버튼을 눌러주세요.\n";
    image_text_guide_string += QString("검열 이미지 폴더의 기본 경로는 `") + QString::fromUtf8(kDefaultImgDirPath.string()) + "` 입니다.";
    image_text_guide_label->setText(image_text_guide_string);
    image_text_guide_label->setStyleSheet(QString(R"(
        QLabel 
        {
            font-size: %1px;
            padding: %2px %3px;
            background-color: transparent;
        }
    )").arg(get_dpi_scaled(14)).arg(get_dpi_scaled(6)).arg(get_dpi_scaled(12)));
    image_tab_text_guide_layout->addWidget(image_text_guide_label);
    
    image_tab_layout->addStretch();
    
    QHBoxLayout* image_tab_button_layout = new QHBoxLayout;
    image_tab_layout->addLayout(image_tab_button_layout);
    image_tab_button_layout->addStretch();

    open_image_dir_button_ = new QPushButton("검열 이미지 폴더 열기", image_setting_tab);
    open_image_dir_button_->setStyleSheet(default_soft_button_style);
    image_tab_button_layout->addWidget(open_image_dir_button_);

    update_censor_image_button_ = new QPushButton("검열 이미지 세트 구성", image_setting_tab);
    update_censor_image_button_->setStyleSheet(default_soft_button_style);
    image_tab_button_layout->addWidget(update_censor_image_button_);

    connect(open_image_dir_button_, &QPushButton::clicked, this, &PolicyPage::open_image_dir_button_clicked);
    connect(update_censor_image_button_, &QPushButton::clicked, this, &PolicyPage::update_censor_image_button_clicked);
    

    // 정규식 탭
    QWidget* regex_setting_tab = new QWidget(this);
    tab_widget->addTab(regex_setting_tab, "정규식");
    
    QVBoxLayout* regex_tab_layout = new QVBoxLayout(regex_setting_tab);
    regex_setting_tab->setLayout(regex_tab_layout);
    
    QLabel* regex_checkbox_header_label = new QLabel("사전 정의된 정규식 종류", regex_setting_tab);
    regex_checkbox_header_label->setStyleSheet(QString(R"(
        QLabel 
        {
            color: #4a66f1;
            font-size: %1px;
            padding: %2px %3px;
            background-color: transparent;
            border-bottom: %4px dashed #ddd;
        }
    )").arg(get_dpi_scaled(16)).arg(get_dpi_scaled(6)).arg(get_dpi_scaled(12)).arg(get_dpi_scaled(1)));
    regex_tab_layout->addWidget(regex_checkbox_header_label);

    QVBoxLayout* regex_check_box_layout = new QVBoxLayout;
    regex_tab_layout->addLayout(regex_check_box_layout);

    QString regex_tab_check_box_style = QString(R"(
        QCheckBox 
        {
            font-size: %1px;
            padding: %2px %3px;
            background-color: transparent;
        }
        QCheckBox::indicator 
        {
            image: url(:/icons/check-box-blank.svg);
        }
        QCheckBox::indicator:checked 
        {
            image: url(:/icons/check-box-checked.svg);
        }
    )").arg(get_dpi_scaled(14)).arg(get_dpi_scaled(12)).arg(get_dpi_scaled(12));
    
    QCheckBox* email_checkbox = new QCheckBox("이메일", regex_setting_tab);
    QCheckBox* phone_checkbox = new QCheckBox("전화번호", regex_setting_tab);
    QCheckBox* ssn_checkbox = new QCheckBox("주민등록번호", regex_setting_tab);
    QCheckBox* ip_port_checkbox = new QCheckBox("IP + 포트", regex_setting_tab);
    QCheckBox* url_checkbox = new QCheckBox("URL", regex_setting_tab);
    QCheckBox* number_checkbox = new QCheckBox("모든 숫자", regex_setting_tab);
    email_checkbox->setProperty("CdsRegex", QVariant::fromValue(static_cast<int>(FilteringInterface::CdsRegex::kEmail)));
    phone_checkbox->setProperty("CdsRegex", QVariant::fromValue(static_cast<int>(FilteringInterface::CdsRegex::kPhoneNumber)));
    ssn_checkbox->setProperty("CdsRegex", QVariant::fromValue(static_cast<int>(FilteringInterface::CdsRegex::kSSNRegex)));
    ip_port_checkbox->setProperty("CdsRegex", QVariant::fromValue(static_cast<int>(FilteringInterface::CdsRegex::kIpWithPortRegex)));
    url_checkbox->setProperty("CdsRegex", QVariant::fromValue(static_cast<int>(FilteringInterface::CdsRegex::kUrlRegex)));
    number_checkbox->setProperty("CdsRegex", QVariant::fromValue(static_cast<int>(FilteringInterface::CdsRegex::kAllNumber)));

    regex_checkbox_arr = { email_checkbox, phone_checkbox, ssn_checkbox, ip_port_checkbox, url_checkbox, number_checkbox} ;
    for (QCheckBox* check_box : regex_checkbox_arr) 
    {
        check_box->setStyleSheet(regex_tab_check_box_style);
        regex_check_box_layout->addWidget(check_box);
    }


    regex_tab_layout->addStretch();

    QHBoxLayout* regex_tab_button_layout = new QHBoxLayout;
    regex_tab_layout->addLayout(regex_tab_button_layout);
    
    regex_tab_button_layout->addStretch();
    QPushButton* regex_apply_button = new QPushButton("적용", regex_setting_tab);
    regex_apply_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    regex_apply_button->setStyleSheet(QString(R"(
        QPushButton 
        {
            font-size: %1px;
            font-weight: bold;
            background-color: #4a66f1; 
            border-radius: %2px;
            padding: %3px %4px;
            color: white;
        }
    )").arg(get_dpi_scaled(14)).arg(get_dpi_scaled(12)).arg(get_dpi_scaled(4)).arg(get_dpi_scaled(36)));
    connect(regex_apply_button, &QPushButton::clicked, this, &PolicyPage::regex_apply_button_clicked);
    regex_tab_button_layout->addWidget(regex_apply_button);



    // 딥러닝 탭
    QWidget* deep_learning_tab = new QWidget(this);
    tab_widget->addTab(deep_learning_tab, "딥러닝");
    QVBoxLayout* deep_learning_tab_layout = new QVBoxLayout(deep_learning_tab);
    deep_learning_tab->setLayout(deep_learning_tab_layout);

    // QHBoxLayout* deep_learning_tab_header_layout = new QHBoxLayout;
    // deep_learning_tab_layout->addLayout(deep_learning_tab_header_layout);
    // QLabel* deep_learning_active_label = new QLabel("활성화");
    // deep_learning_active_label->setStyleSheet(QString(R"(
    //     QLabel 
    //     {
    //         color: #4a66f1;
    //         font-size: %1px;
    //         padding: %2px %3px;
    //         background-color: transparent;
    //         border-bottom: %4px dashed #ddd;
    //     }
    // )").arg(get_dpi_scaled(16)).arg(get_dpi_scaled(6)).arg(get_dpi_scaled(12)).arg(get_dpi_scaled(1)));
    // deep_learning_tab_header_layout->addWidget(deep_learning_active_label);
    // deep_learning_tab_header_layout->addStretch();

    QHBoxLayout* deep_learning_active_layout = new QHBoxLayout;
    deep_learning_tab_layout->addLayout(deep_learning_active_layout);
    // deep_learning_active_layout->setContentsMargins(get_dpi_scaled(15), get_dpi_scaled(5), get_dpi_scaled(15), get_dpi_scaled(15));

    QLabel* deep_learning_train_list_header_label = new QLabel("훈련 문서", prohibited_setting_page);
    int deep_learning_docs_list_bottom_px = get_dpi_scaled(1);
    int deep_learning_docs_list_padding_size = get_dpi_scaled(8);
    deep_learning_train_list_header_label->setStyleSheet(QString(R"(
        QLabel 
        {
            font-size: %1px;
            font-weight: bold;
            color: #4a66f1;
            padding: %2px %2px;
        }
    )").arg(get_dpi_scaled(16)).arg(deep_learning_docs_list_padding_size));
    deep_learning_active_layout->addWidget(deep_learning_train_list_header_label);
    
    deep_learning_active_layout->addStretch();

    deep_learning_active_button_ = new QPushButton("딥러닝   활성화", this);
    deep_learning_active_button_->setStyleSheet(default_soft_button_style);
    deep_learning_active_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    connect(deep_learning_active_button_, &QPushButton::clicked, this, &PolicyPage::deep_learning_active_button_clicked);
    deep_learning_active_layout->addWidget(deep_learning_active_button_);
    
    deep_learning_inactive_button_ = new QPushButton("딥러닝 비활성화", this);
    deep_learning_inactive_button_->setStyleSheet(default_soft_button_style);
    deep_learning_inactive_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    connect(deep_learning_inactive_button_, &QPushButton::clicked, this, &PolicyPage::deep_learning_inactive_button_clicked);
    deep_learning_active_layout->addWidget(deep_learning_inactive_button_);
    
    deep_learning_docs_dir_open_button_ = new QPushButton("훈련 데이터 폴더 열기", this);
    deep_learning_docs_dir_open_button_->setStyleSheet(default_soft_button_style);
    deep_learning_docs_dir_open_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    connect(deep_learning_docs_dir_open_button_, &QPushButton::clicked, this, &PolicyPage::deep_learning_docs_dir_open_button_clicked);
    deep_learning_active_layout->addWidget(deep_learning_docs_dir_open_button_);

    QVBoxLayout* deep_learning_list_layout = new QVBoxLayout;
    deep_learning_tab_layout->addLayout(deep_learning_list_layout);
    deep_learning_list_layout->setSpacing(0);
    
    deep_learning_docs_list_widget_ = new QListWidget(deep_learning_tab);
    deep_learning_docs_list_widget_->setContextMenuPolicy(Qt::CustomContextMenu);
    deep_learning_docs_list_widget_->setStyleSheet(QString(R"(
        QListWidget 
        {
            border-left: none;
            border-right: none;
            border-top: none;
            border-bottom: 1px solid #e0e0e0;
        }
        QListWidget::item 
        {
            border-bottom: %1px solid #dddddd;
            padding: %2px;
        }
        
        QListWidget::item:selected 
        {
            background-color: #EEF0FF;
            border-bottom: %1px solid #dddddd;
            color: #000000;
        }
        
        QListWidget::item:hover 
        {
            background-color: #f5f7fa;
            color: #000;
        }

        QListWidget::item:selected:active 
        {
            outline: none;
        }
    )").arg(deep_learning_docs_list_bottom_px).arg(deep_learning_docs_list_padding_size));
    deep_learning_list_layout->addWidget(deep_learning_docs_list_widget_);
    connect(deep_learning_docs_list_widget_, &QListWidget::customContextMenuRequested, this, &PolicyPage::deep_learning_docs_list_right_clicked);

    QWidget* deep_learning_docs_list_bottom_widget = new QWidget(deep_learning_tab);
    QString deep_learning_list_bg_color = deep_learning_docs_list_widget_->palette().color(QPalette::Base).name();
    deep_learning_docs_list_bottom_widget->setStyleSheet(QString("background-color: %1;").arg(deep_learning_list_bg_color)); // 리스트와 같은 색상
    QHBoxLayout* deep_learning_docs_list_bottom_layout_ = new QHBoxLayout(deep_learning_docs_list_bottom_widget);
    deep_learning_docs_list_bottom_layout_->setContentsMargins(0, 0, 0, 0);

    deep_learning_list_layout->addWidget(deep_learning_docs_list_bottom_widget);

    QToolButton* deep_learning_docs_list_refresh_button_ = new QToolButton(deep_learning_tab);
    deep_learning_docs_list_refresh_button_->setIconSize(QSize(get_dpi_scaled(16), get_dpi_scaled(16)));
    deep_learning_docs_list_refresh_button_->setAutoRaise(true);
    deep_learning_docs_list_refresh_button_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    deep_learning_docs_list_refresh_button_->setStyleSheet(QString(R"(
        QToolButton {
            border: none;
            background: transparent;
            image: url(:/icons/refresh.svg);
            padding: %1px;
        }
        QToolButton:hover {
            image: url(:/icons/refresh-hover.svg);
        }
    )").arg(get_dpi_scaled(4)));
    connect(deep_learning_docs_list_refresh_button_, &QToolButton::clicked, this, &PolicyPage::deep_learning_docs_list_refresh_button_clicked);

    QHBoxLayout* server_input_layout = new QHBoxLayout();
    QLabel* ip_address_label = new QLabel("Server IP:", deep_learning_tab);
    ip_address_edit_ = new QLineEdit(deep_learning_tab);
    
    QLabel* port_label = new QLabel("Port:", deep_learning_tab);
    port_spin_box_ = new QSpinBox(deep_learning_tab);
    port_spin_box_->setRange(1, 65535);
    port_spin_box_->setValue(8000);

    deep_learning_server_endpoint_set_button_ = new QPushButton("설정", deep_learning_tab);
    deep_learning_server_endpoint_set_button_->setStyleSheet(default_soft_button_style);
    connect(deep_learning_server_endpoint_set_button_, &QPushButton::clicked, this, &PolicyPage::deep_learning_server_endpoint_set_button_clicked);
    
    deep_learning_train_data_send_button_ = new QPushButton("훈련 데이터 전송", deep_learning_tab);
    deep_learning_train_data_send_button_->setStyleSheet(default_soft_button_style);
    connect(deep_learning_train_data_send_button_, &QPushButton::clicked, this, &PolicyPage::deep_learning_train_data_send_button_clicked);
    
    server_input_layout->addWidget(ip_address_label);
    server_input_layout->addWidget(ip_address_edit_);
    server_input_layout->addSpacing(10);
    server_input_layout->addWidget(port_label);
    server_input_layout->addWidget(port_spin_box_);
    server_input_layout->addWidget(deep_learning_server_endpoint_set_button_);
    server_input_layout->addWidget(deep_learning_train_data_send_button_);
    deep_learning_docs_list_bottom_layout_->addLayout(server_input_layout);

    deep_learning_docs_list_bottom_layout_->addStretch();
    deep_learning_docs_list_bottom_layout_->addWidget(deep_learning_docs_list_refresh_button_);
    // deep_learning_tab_layout->addStretch();
    
    
    
    // 기본 file load 및 trie 빌드
    
    if (!std::filesystem::exists(kDefaultDicPath))
    {
        std::filesystem::create_directories(kDefaultDicPath.parent_path());
        
        if (std::ofstream file(kDefaultDicPath); file.is_open() == false)
        {
            QMessageBox::critical(this, "에러", "data 폴더에 접근할 수 없습니다.\n권한을 확인하십시오.");
        }
    }
    
    if (!std::filesystem::exists(kDefaultImgDirPath))
    {
        if (std::filesystem::create_directories(kDefaultImgDirPath) == false)
        {
            QMessageBox::critical(this, "에러", "data 폴더에 접근할 수 없습니다.\n권한을 확인하십시오.");
        }
    }
    
    if (!std::filesystem::exists(kDefaultTrainDocsDirPath))
    {
        if (std::filesystem::create_directories(kDefaultTrainDocsDirPath) == false)
        {
            QMessageBox::critical(this, "에러", "data 폴더에 접근할 수 없습니다.\n권한을 확인하십시오.");
        }
    }
    
    if (!std::filesystem::exists(kDefaultTrainDocsLabelOptionPath))
    {
        if (std::ofstream file(kDefaultTrainDocsLabelOptionPath); file.is_open() == false)
        {
            QMessageBox::critical(this, "에러", "data 폴더에 접근할 수 없습니다.\n권한을 확인하십시오.");
        }
    }
    
    load_patterns_from_file(kDefaultDicPath);
    build_filtering_interface();
    load_labels_from_file(kDefaultTrainDocsLabelOptionPath);
    if (label_set_.empty())
    {
        label_set_.insert("SECRET");
    }
    deep_learning_docs_list_refresh_button_clicked();
    manager_ = new QNetworkAccessManager(this);
    
    setLayout(main_layout);
}

void PolicyPage::refresh_prohibited_word_list_widget()
{
    prohibited_word_list_widget_->clear();

    int start = prohibited_list_current_page_ * prohibited_list_items_per_page_;
    int end = start + prohibited_list_items_per_page_;
    int i = 0, shown = 0;

    for (auto it = pattern_set_.begin(); it != pattern_set_.end(); ++it)
    {
        if (!will_erase_pattern_set_.contains(*it))
        {
            if (i >= start && i < end)
            {
                prohibited_word_list_widget_->addItem(make_new_item(*it));
                shown++;
            }

            i++;
        }

        if (shown >= prohibited_list_items_per_page_) break;
    }

    if (shown < prohibited_list_items_per_page_)
    {
        for (auto it = new_inserted_pattern_set_.begin(); it != new_inserted_pattern_set_.end(); ++it)
        {
            if (i >= start && i < end)
            {
                prohibited_word_list_widget_->addItem(make_new_item(*it));
                shown++;
            }

            i++;

            if (shown >= prohibited_list_items_per_page_) break;
        }
    }

    update_page_buttons();
    update_total_count_label();
}

void PolicyPage::apply_filter(const QString& _keyword)
{
    
    prohibited_word_list_widget_->clear();
    if (_keyword.isEmpty())
    {
        refresh_prohibited_word_list_widget();
        return;
    }
    
    auto it_curr = pattern_set_.begin();
    auto it_end = pattern_set_.end();
    
    std::string keyword_std_string = _keyword.toStdString();

    for (; it_curr != it_end && !will_erase_pattern_set_.contains(*it_curr); it_curr++)
    {
        if (it_curr->contains(keyword_std_string))
            prohibited_word_list_widget_->addItem(make_new_item(*it_curr));
    }

    for (const std::string& std_str : new_inserted_pattern_set_)
    {
        if (std_str.contains(keyword_std_string))
            prohibited_word_list_widget_->addItem(make_new_item(std_str));
    }
}

void PolicyPage::search_button_clicked()
{
    apply_filter(search_input_->text().trimmed());
}

void PolicyPage::add_button_clicked()
{
    bool ok = false;
    QString input_text = QInputDialog::getText(
        this,
        "금칙어 추가",
        "등록하고 싶은 문자열을 입력해주세요:",
        QLineEdit::Normal,
        "",
        &ok
    );

    if (!ok || input_text.trimmed().isEmpty())
        return;

    std::string std_str = input_text.trimmed().toStdString();

    if (pattern_set_.contains(std_str) || new_inserted_pattern_set_.contains(std_str))
    {
        QMessageBox::information(this, "정보", "이미 존재하는 금칙어입니다.");
        return;
    }

    new_inserted_pattern_set_.insert(std_str);
    mark_pattern_modified();
    refresh_prohibited_word_list_widget();
}

void PolicyPage::prohibit_word_list_right_clicked(const QPoint& _pos) 
{
    QMenu menu(this);
    QAction* edit_action = menu.addAction("수정");
    QAction* delete_action = menu.addAction("삭제");
    QAction* selected_action = menu.exec(prohibited_word_list_widget_->viewport()->mapToGlobal(_pos));

    if (selected_action == edit_action) 
    {
        prohibited_word_edit_button_clicked();
    }
    else if (selected_action == delete_action) 
    {
        prohibited_word_remove_button_clicked();
    }
}

void PolicyPage::prohibited_word_remove_button_clicked()
{
    auto* item = prohibited_word_list_widget_->currentItem();
    if (!item)
        return;

    QString text = item->text();
    std::string std_str = text.toStdString();
    
    std::set<std::string>::iterator it = pattern_set_.find(std_str);
    if (it != pattern_set_.end())
    {
        will_erase_pattern_set_.insert(*(pattern_set_.find(std_str)));
    }
    else if (new_inserted_pattern_set_.contains(std_str))
    {
        new_inserted_pattern_set_.erase(std_str);
    }

    mark_pattern_modified();
    refresh_prohibited_word_list_widget();
}

void PolicyPage::prohibited_word_edit_button_clicked()
{
    QListWidgetItem* item = prohibited_word_list_widget_->currentItem();
    if (!item) return;

    item->setData(Qt::UserRole, item->text());

    prohibited_word_list_widget_->editItem(item);
}

void PolicyPage::edit_list_item(QListWidgetItem* _list_item)
{
    QString new_qstr = _list_item->text();
    std::string new_std_str = new_qstr.toStdString();

    if (new_std_str.empty()) return;

    std::string old_std_str = _list_item->data(Qt::UserRole).toString().toStdString();

    if (old_std_str.empty() || old_std_str == new_std_str) return;

    if (pattern_set_.contains(old_std_str)) {
        pattern_set_.erase(old_std_str);
        new_inserted_pattern_set_.insert(new_std_str);
    } else {
        new_inserted_pattern_set_.erase(old_std_str);
        new_inserted_pattern_set_.insert(new_std_str);
    }

    mark_pattern_modified();

    _list_item->setData(Qt::UserRole, QVariant());
}

void PolicyPage::undo_button_clicked()
{
    new_inserted_pattern_set_.clear();
    will_erase_pattern_set_.clear();
    build_button_->setEnabled(false);
    refresh_prohibited_word_list_widget();
}

void PolicyPage::build_button_clicked()
{
    update_pattern_set();
    build_filtering_interface();
    save_patterns_to_file(kDefaultDicPath);
}

void PolicyPage::build_filtering_interface()
{
    if (is_building_) return;

    is_building_ = true;
    build_button_->setEnabled(false);

    QMessageBox* box = new QMessageBox(
        QMessageBox::Information,
        "빌드 중", "DoubleArrayTrie를 생성 중입니다...\n완료되면 자동으로 닫힙니다.",
        QMessageBox::NoButton, this
    );
    box->setModal(true);
    box->show();

    std::thread([this, box]() mutable 
    {
        try 
        {
            FilteringInterface::build_all(pattern_set_, get_cds_regex_enum(), load_image_dir_to_vec());
        } 
        catch (const std::exception& e) {
            QMetaObject::invokeMethod(this, [this, box, msg = QString::fromStdString(e.what())]() {
                box->hide();
                box->deleteLater();
                QMessageBox::critical(
                    this, 
                    "에러", 
                    "빌드 중 오류 발생: " + msg + '\n' + "에러 원인을 제거 후 다시 빌드 버튼을 누르십시오.\n(Trie 깨짐으로 필터링 중지. 필터링을 위해 재구성 필요!)"
                );
                build_button_->setEnabled(true);
                is_building_ = false;
            }, Qt::QueuedConnection);
            return;
        }

        QMetaObject::invokeMethod(this, 
            [this, box]() 
            {
                box->hide();
                box->deleteLater();
                build_button_->setEnabled(false);
                is_building_ = false;
                QMessageBox::information(this, "완료", "DoubleArrayTrie 빌드가 완료되었습니다.");
            }, 
        Qt::QueuedConnection);
    }).detach();
}

void PolicyPage::mark_pattern_modified()
{
    pattern_modified_flag_ = true;
    build_button_->setEnabled(true);
}

void PolicyPage::load_patterns_from_file(const std::filesystem::path& _file_path)
{
    std::ifstream in(_file_path, std::ios::binary);
    if (!in)
    {
        QMessageBox::warning(this, "오류", "파일을 열 수 없습니다.");
        return;
    }

    char bom[3] = {};
    in.read(bom, 3);
    std::streamsize bytes_read = in.gcount();
    bool has_bom = (bytes_read == 3 && static_cast<unsigned char>(bom[0]) == 0xEF && static_cast<unsigned char>(bom[1]) == 0xBB && static_cast<unsigned char>(bom[2]) == 0xBF);

    in.clear();
    in.seekg(has_bom ? 3 : 0);

    std::string test_line;
    std::getline(in, test_line);

    bool is_valid_utf8 = !QString::fromUtf8(test_line.c_str()).isNull();

    if (!is_valid_utf8)
    {
        QMessageBox::warning(this, "경고", "이 파일은 UTF-8 형식이 아닐 수 있습니다.\n글자가 깨질 수 있습니다.");
        in.clear();
        in.seekg(0);
    }
    else
    {
        in.clear();
        in.seekg(has_bom ? 3 : 0);
    }

    if (pattern_modified_flag_)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "경고",
            "저장되지 않은 금칙어 변경사항이 있습니다.\n불러오면 기존 변경사항이 사라집니다.\n계속 진행하시겠습니까?",
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply != QMessageBox::Yes)
        {
            return;
        }
    }

    pattern_set_.clear();
    new_inserted_pattern_set_.clear();
    will_erase_pattern_set_.clear();

    std::string line;
    while (std::getline(in, line))
    {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty())
        {
            pattern_set_.insert(std::move(line));
        }
    }

    prohibited_list_current_page_ = 0;
    refresh_prohibited_word_list_widget();
    mark_pattern_modified();
}

void PolicyPage::save_patterns_to_file(const std::filesystem::path& _file_path)
{
    std::ofstream pattern_set_ofstream(_file_path, std::ios::binary);

    if (!pattern_set_ofstream)
    {
        QMessageBox::warning(this, "오류", "금칙어 파일을 저장하지 못했습니다.");
        return;
    }

    for (std::string_view output_string : pattern_set_)
    {
        pattern_set_ofstream << output_string << std::endl;
    }

    pattern_set_ofstream.close();
}

void PolicyPage::update_page_buttons()
{
    QLayout* layout = page_control_widget_->layout();
    if (layout)
    {
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete layout;
    }

    const int total_items = pattern_set_.size() + new_inserted_pattern_set_.size() - will_erase_pattern_set_.size();
    const int total_pages = (total_items + prohibited_list_items_per_page_ - 1) / prohibited_list_items_per_page_;
    const int max_page_buttons = 5;

    int padding_v = get_dpi_scaled(4);
    int padding_h = get_dpi_scaled(8);
    int radius = get_dpi_scaled(3);
    int font_size = get_dpi_scaled(14);

    QString page_button_style = QString(R"(
        QPushButton {
            background-color: transparent;
            border: none;
            border-radius: %1px;
            padding: %2px %3px;
            color: #444;
            font-size: %4px;
        }
        QPushButton:hover:enabled {
            background-color: #4a66f1;
            color: white;
        }
    )").arg(radius).arg(padding_v).arg(padding_h).arg(font_size);

    auto* layout_new = new QHBoxLayout;
    layout_new->addStretch();

    // ⏮ 처음 페이지
    QPushButton* first_btn = new QPushButton("|<");
    first_btn->setFlat(true);
    first_btn->setEnabled(prohibited_list_current_page_ > 0);
    first_btn->setStyleSheet(page_button_style);
    connect(first_btn, &QPushButton::clicked, this, [this]() {
        prohibited_list_current_page_ = 0;
        refresh_prohibited_word_list_widget();
    });
    layout_new->addWidget(first_btn);

    // ◀ 이전 페이지
    QPushButton* prev_btn = new QPushButton("◀");
    prev_btn->setFlat(true);
    prev_btn->setEnabled(prohibited_list_current_page_ > 0);
    prev_btn->setStyleSheet(page_button_style);
    connect(prev_btn, &QPushButton::clicked, this, [this]() {
        if (prohibited_list_current_page_ > 0) {
            prohibited_list_current_page_--;
            refresh_prohibited_word_list_widget();
        }
    });
    layout_new->addWidget(prev_btn);

    // 숫자 페이지 버튼들
    int start = std::max(0, prohibited_list_current_page_ - 2);
    int end = std::min(total_pages, start + max_page_buttons);
    start = std::max(0, end - max_page_buttons);

    for (int i = start; i < end; ++i)
    {
        QPushButton* btn = new QPushButton(QString::number(i + 1));
        btn->setFlat(true);
        btn->setStyleSheet(page_button_style);

        if (i == prohibited_list_current_page_)
            btn->setEnabled(false);  // 강조 색상은 :disabled 스타일로 표현

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            prohibited_list_current_page_ = i;
            refresh_prohibited_word_list_widget();
        });

        layout_new->addWidget(btn);
    }

    // ▶ 다음 페이지
    QPushButton* next_btn = new QPushButton("▶");
    next_btn->setFlat(true);
    next_btn->setEnabled(prohibited_list_current_page_ < total_pages - 1);
    next_btn->setStyleSheet(page_button_style);
    connect(next_btn, &QPushButton::clicked, this, [this, total_pages]() {
        if (prohibited_list_current_page_ < total_pages - 1) {
            prohibited_list_current_page_++;
            refresh_prohibited_word_list_widget();
        }
    });
    layout_new->addWidget(next_btn);

    // ⏭ 마지막 페이지
    QPushButton* last_btn = new QPushButton(">|");
    last_btn->setFlat(true);
    last_btn->setEnabled(prohibited_list_current_page_ < total_pages - 1);
    last_btn->setStyleSheet(page_button_style);
    connect(last_btn, &QPushButton::clicked, this, [this, total_pages]() {
        prohibited_list_current_page_ = total_pages - 1;
        refresh_prohibited_word_list_widget();
    });
    layout_new->addWidget(last_btn);

    layout_new->addStretch();
    page_control_widget_->setLayout(layout_new);
}

void PolicyPage::update_total_count_label()
{
    int total_count = static_cast<int>(pattern_set_.size() + new_inserted_pattern_set_.size() - will_erase_pattern_set_.size());

    if (total_count_label_)
        total_count_label_->setText(QString("%1개 항목").arg(total_count));

}

void PolicyPage::update_pattern_set()
{
    for (const std::string& erase_string : will_erase_pattern_set_)
    {
        pattern_set_.erase(erase_string);
    }

    will_erase_pattern_set_.clear();

    pattern_set_.merge(new_inserted_pattern_set_);

    pattern_modified_flag_ = false;
    build_button_->setEnabled(false);
}

void PolicyPage::load_labels_from_file(const std::filesystem::path& _file_path)
{
    std::ifstream in(_file_path, std::ios::binary);
    if (!in)
    {
        QMessageBox::warning(this, "오류", "파일을 열 수 없습니다.");
        return;
    }

    char bom[3] = {};
    in.read(bom, 3);
    std::streamsize bytes_read = in.gcount();
    bool has_bom = (bytes_read == 3 && static_cast<unsigned char>(bom[0]) == 0xEF && static_cast<unsigned char>(bom[1]) == 0xBB && static_cast<unsigned char>(bom[2]) == 0xBF);

    in.clear();
    in.seekg(has_bom ? 3 : 0);

    std::string line;
    while (std::getline(in, line))
    {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty())
        {
            label_set_.insert(QString::fromUtf8(line));
        }
    }
}

void PolicyPage::save_labels_to_file(const std::filesystem::path& _file_path)
{
    std::ofstream out(_file_path, std::ios::binary);
    if (!out)
    {
        QMessageBox::warning(this, "오류", "파일을 저장할 수 없습니다.");
        return;
    }

    // UTF-8 BOM 작성 (선택 사항)
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    out.write(reinterpret_cast<const char*>(bom), sizeof(bom));

    for (const QString& label : label_set_)
    {
        QByteArray utf8 = label.trimmed().toUtf8();
        out.write(utf8.constData(), utf8.size());
        out.put('\n');
    }
}

std::vector<cv::Mat> PolicyPage::load_image_dir_to_vec()
{
    std::vector<cv::Mat> image_vec;

    if (!std::filesystem::exists(kDefaultImgDirPath) || !std::filesystem::is_directory(kDefaultImgDirPath))
    {
        std::cerr << "Image directory does not exist: " << kDefaultImgDirPath << std::endl;
        return image_vec;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(kDefaultImgDirPath)) 
    {
        if (!entry.is_regular_file()) continue;

        const auto& path = entry.path();
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") 
        {
            cv::Mat image = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
            if (!image.empty()) {
                image_vec.emplace_back(std::move(image));
            } 
            else 
            {
                std::cerr << "Failed to load image: " << path << std::endl;
            }
        }
    }

    return image_vec;
}

void PolicyPage::load_button_clicked()
{
    QString q_string_path = QFileDialog::getOpenFileName(this, "금칙어 파일 불러오기", "", "Text Files (*.txt)");
    std::filesystem::path std_path = q_string_path.toStdString();
    if (!std_path.empty()) load_patterns_from_file(std_path);
}

QListWidgetItem* PolicyPage::make_new_item(const QString& _string)
{
    QListWidgetItem* item = new QListWidgetItem(_string);
    item->setFlags(item->flags() | Qt::ItemIsEditable);

    return item;
}

QListWidgetItem* PolicyPage::make_new_item(std::string_view _string)
{
    QListWidgetItem* item = new QListWidgetItem(QString::fromUtf8(_string));
    item->setFlags(item->flags() | Qt::ItemIsEditable);

    return item;
}

std::vector<FilteringInterface::CdsRegex> PolicyPage::get_cds_regex_enum()
{
    std::vector<FilteringInterface::CdsRegex> regex_enum_vec;

    for (QCheckBox* checkbox : regex_checkbox_arr) 
    {
        if (checkbox->isChecked()) 
        {
            int type_value = checkbox->property("CdsRegex").toInt();
            regex_enum_vec.push_back(static_cast<FilteringInterface::CdsRegex>(type_value));
        }
    }

    return regex_enum_vec;
}

void PolicyPage::regex_apply_button_clicked()
{
    FilteringInterface::build_regex(get_cds_regex_enum());
    QMessageBox::information(this, "완료", "정규식이 적용 되었습니다.");
}

int PolicyPage::get_dpi_scaled(int _px) const
{
    static const qreal scale = QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0;
    return int(std::round(_px * scale));
}

void PolicyPage::open_image_dir_button_clicked() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(kDefaultImgDirPath.c_str()));
}

void PolicyPage::update_censor_image_button_clicked()
{
    is_building_ = true;
    bool is_build_button_able = build_button_->isEnabled();
    build_button_->setEnabled(false);
    
    QMessageBox* box = new QMessageBox(
        QMessageBox::Information,
        "빌드 중", "이미지 셋을 업데이트 중입니다...\n완료되면 자동으로 닫힙니다.",
        QMessageBox::NoButton, this
    );
    box->setModal(true);
    box->show();

    std::thread([this, box, is_build_button_able]() mutable 
    {
        try 
        {
            FilteringInterface::build_image_hash_set(load_image_dir_to_vec());
        } 
        catch (const std::exception& e) {
            QMetaObject::invokeMethod(this, [this, box, msg = QString::fromStdString(e.what()), is_build_button_able]() {
                box->hide();
                box->deleteLater();
                QMessageBox::critical(
                    this, 
                    "에러", 
                    "검열 이미지 셋 업데이트 중 오류 발생: " + msg + '\n' + ")"
                );
                build_button_->setEnabled(is_build_button_able);
                is_building_ = false;
            }, Qt::QueuedConnection);
            return;
        }

        QMetaObject::invokeMethod(this, 
            [this, box, is_build_button_able]() 
            {
                box->hide();
                box->deleteLater();
                build_button_->setEnabled(is_build_button_able);
                is_building_ = false;
                QMessageBox::information(this, "완료", "검열 이미지 셋 업데이트가 완료되었습니다.");
            }, 
        Qt::QueuedConnection);
    }).detach();
}

void PolicyPage::deep_learning_docs_dir_open_button_clicked() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(kDefaultTrainDocsDirPath.c_str()));
}


void PolicyPage::deep_learning_active_button_clicked()
{
    FilteringInterface& filtering_interface = FilteringInterface::instance().get();
    if (!filtering_interface.is_deep_learning_enabled())
    {
        filtering_interface.deep_learning_enable();
    }

}

void PolicyPage::deep_learning_inactive_button_clicked()
{
    FilteringInterface& filtering_interface = FilteringInterface::instance().get();
    if (filtering_interface.is_deep_learning_enabled())
    {
        filtering_interface.deep_learning_disable();
    }
}

void PolicyPage::deep_learning_server_endpoint_set_button_clicked()
{
    QString ip_address = ip_address_edit_->text().trimmed();
    int port = port_spin_box_->value();

    if (ip_address.isEmpty() || QHostAddress(ip_address).isNull() || port <= 0 || port > 65535)
    {
        // 유효하지 않은 IP 주소 또는 포트
        QMessageBox::warning(this, "오류", "유효한 IP 주소와 포트를 입력하세요.");
        return;
    }

    deep_learning_server_ip_ = ip_address;
    deep_learning_server_port_ = port;

    FilteringInterface& filtering_interface = FilteringInterface::instance().get();
    filtering_interface.set_server_endpoint(ip_address.toStdString(), port);
}

void PolicyPage::deep_learning_train_data_send_button_clicked()
{
    send_all_text_training_data();
}

void PolicyPage::deep_learning_docs_list_refresh_button_clicked()
{
    deep_learning_docs_list_widget_->clear();

    const std::filesystem::path docs_dir = kDefaultTrainDocsDirPath;

    if (!std::filesystem::exists(docs_dir) || !std::filesystem::is_directory(docs_dir))
        return;

    for (const auto& entry : std::filesystem::directory_iterator(docs_dir)) {
        if (!entry.is_regular_file())
            continue;

        const auto& path = entry.path();

        // 원하는 확장자만 필터링 (예: docx, pdf, hwp 등)
        const std::string ext = path.extension().string();
        if (ext == ".hwpx" || ext == ".docx" || ext == ".xlsx") 
        {
            QString filename = QString::fromStdString(path.filename().string());

            auto* item = new QListWidgetItem(filename);
            deep_learning_docs_list_widget_->addItem(item);
        }
    }
}

void PolicyPage::deep_learning_docs_list_right_clicked(const QPoint& _pos) 
{
    QMenu menu(this);
    QAction* edit_action = menu.addAction("라벨링");
    QAction* delete_action = menu.addAction("삭제");
    QAction* selected_action = menu.exec(deep_learning_docs_list_widget_->viewport()->mapToGlobal(_pos));

    if (selected_action == edit_action) 
    {
        deep_learning_docs_labeling_clicked();
    }
    else if (selected_action == delete_action) 
    {
        deep_learning_docs_remove_clicked();
    }
}

void PolicyPage::deep_learning_docs_remove_clicked()
{
    QListWidgetItem* item = deep_learning_docs_list_widget_->currentItem();
    if (!item) return;

    QString filename = item->text();
    std::filesystem::path file_path = kDefaultTrainDocsDirPath / filename.toStdString();

    if (!std::filesystem::exists(file_path))
    {
        QMessageBox::warning(this, "오류", "선택한 파일이 존재하지 않습니다.");
        return;
    }

    if (QMessageBox::question(this, "삭제 확인", "선택한 파일을 삭제하시겠습니까?") == QMessageBox::Yes)
    {
        try
        {
            std::filesystem::remove(file_path);
            int row = deep_learning_docs_list_widget_->row(item);
            delete deep_learning_docs_list_widget_->takeItem(row);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            QMessageBox::critical(this, "삭제 실패", QString("파일 삭제 중 오류 발생:\n%1").arg(e.what()));
        }
    }
}

void PolicyPage::deep_learning_docs_labeling_clicked()
{
    QListWidgetItem* item = deep_learning_docs_list_widget_->currentItem();
    if (!item) return;

    QString filename = item->text();
    std::filesystem::path doc_file_path = kDefaultTrainDocsDirPath / filename.toStdString();

    if (!std::filesystem::exists(doc_file_path)) 
    {
        QMessageBox::warning(this, "오류", "선택한 파일이 존재하지 않습니다.");
        return;
    }

    std::ifstream ifs(doc_file_path, std::ios::binary);
    if (!ifs)
    {
        QMessageBox::critical(this, "에러", "파일을 열 수 없습니다.");
        return;
    }

    std::vector<std::uint8_t> buffer(std::istreambuf_iterator<char>(ifs), {});
    std::span<std::uint8_t> file_span(buffer.data(), buffer.size());

    std::expected<ParseZip::RawExtractedDocContent, ParseZip::ExtractErrorInfo> result = ParseZip::extract_doc_content(file_span);
    if (!result.has_value())
    {
        const auto& error = result.error();
        QMessageBox::critical(this, "파싱 실패", QString("문서 파싱 실패: %1").arg(static_cast<int>(error.code)));
        return;
    }

    std::expected<QJsonArray, std::error_code> label_data_expected = load_text_labeling_data(doc_file_path);
    QJsonArray text_labels;

    if (!label_data_expected.has_value())
    {
        if (label_data_expected.error() == std::errc::no_such_file_or_directory)
        {
            QMessageBox::information(this, "정보", "라벨링 데이터가 없습니다. 새로 생성합니다.");
        }
        else if (label_data_expected.error() == std::errc::invalid_argument)
        {
            QMessageBox::critical(this, "오류", "라벨링 데이터 형식이 잘못되었습니다. 새로 생성합니다.");
        }
        else if (label_data_expected.error() == std::errc::protocol_error)
        {
            QMessageBox::warning(this, "경고", "문서와 라벨링 데이터의 해시가 일치하지 않습니다. 새로 생성합니다.");
        }
        
        DocumentLabelingDialog dlg(label_set_, result.value(), this);
        if (dlg.exec() == QDialog::Accepted)
        {
            text_labels = dlg.get_text_labels_with_none();
        }
    }
    else
    {
        QJsonArray label_array = label_data_expected.value();

        DocumentLabelingDialog dlg(label_set_, label_array, {}, this);
        if (dlg.exec() == QDialog::Accepted)
        {
            text_labels = dlg.get_text_labels_with_none();
        }
    }

    QJsonDocument doc(text_labels);
    // qDebug().noquote() << doc.toJson(QJsonDocument::Indented);
    save_text_labeling_data(doc_file_path, text_labels);
}

void PolicyPage::save_text_labeling_data(const std::filesystem::path& _doc_file_path, const QJsonArray& label_array)
{
    QFile doc_file(QString::fromStdString(_doc_file_path.string()));
    if (!doc_file.open(QIODevice::ReadOnly)) 
    {
        qWarning("문서 파일 열기 실패");
        return;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&doc_file);
    QByteArray file_hash = hash.result();
    doc_file.close();

    QJsonObject root;
    root["file_hash"] = QString::fromUtf8(file_hash.toHex());
    root["labels"] = label_array;
    
    std::filesystem::path json_path = _doc_file_path;
    json_path += ".json";

    QFile json_file(QString::fromStdString(json_path.string()));
    if (!json_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("JSON 파일 저장 실패");
        return;
    }

    QJsonDocument doc(root);
    json_file.write(doc.toJson(QJsonDocument::Indented));
}

std::expected<QJsonArray, std::error_code> PolicyPage::load_text_labeling_data(std::filesystem::path _json_file_path)
{
    if (_json_file_path.empty())
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    else if (_json_file_path.extension() != ".json")
        _json_file_path += ".json";

    QFile json_file(QString::fromStdString(_json_file_path.string()));
    if (!json_file.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));

    QByteArray json_data = json_file.readAll();
    json_file.close();

    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(json_data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !doc.isObject())
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));

    QJsonObject root = doc.object();
    if (!root.contains("file_hash") || !root.contains("labels"))
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));

    QString json_hash_str = root["file_hash"].toString();
    QJsonArray label_array = root["labels"].toArray();

    // 문서 경로 추정
    std::filesystem::path doc_path = _json_file_path;
    doc_path.replace_extension(); // "file.hwpx.json" → "file.hwpx"

    QFile doc_file(QString::fromStdString(doc_path.string()));
    if (!doc_file.open(QIODevice::ReadOnly))
        return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&doc_file);
    QByteArray current_hash = hash.result();
    doc_file.close();

    if (current_hash.toHex() != json_hash_str.toUtf8())
        return std::unexpected(std::make_error_code(std::errc::protocol_error));

    return label_array;
}

void PolicyPage::send_text_training_data(const QJsonArray& _training_text_data)
{
    // 1. QJsonArray를 QJsonDocument로 감싸고, 다시 바이트 배열(UTF-8)로 변환합니다.
    //    이것이 HTTP Body에 실릴 실제 데이터입니다.
    QJsonArray filtered_array;
    for (const auto& value : _training_text_data)
    {
        if (!value.isObject()) continue;
        QJsonObject obj = value.toObject();

        // annotations 배열이 존재하면 처리
        if (obj.contains("annotations") && obj["annotations"].isArray()) {
            QJsonArray annotations = obj["annotations"].toArray();
            bool has_non_none = false;

            for (const auto& anno : annotations) {
                if (anno.isObject()) {
                    QJsonObject annoObj = anno.toObject();
                    if (annoObj["label"].toString() != "NONE") {
                        has_non_none = true;
                        break;
                    }
                }
            }

            if (has_non_none)
                filtered_array.append(obj); // ⬅ "NONE" 외 라벨이 있으면 포함
        }
    }
    QJsonDocument doc(filtered_array);
    QByteArray jsonData = doc.toJson();

    // 2. QNetworkRequest 객체를 생성하고, 보낼 서버의 URL을 지정합니다.
    //    이 URL은 Python FastAPI 서버에서 정의할 학습 데이터 수신용 엔드포인트입니다.
    QString url_str = QString("http://%1:%2/add_text_training_data").arg(deep_learning_server_ip_).arg(deep_learning_server_port_);
    QNetworkRequest request{QUrl(url_str)};
    qDebug() << "Sending training data to:" << url_str;

    // 3. HTTP 헤더를 설정합니다. 이것은 매우 중요합니다.
    //    서버에게 "내가 지금 보내는 데이터는 JSON 형식이야" 라고 알려주는 역할을 합니다.
    // qDebug().noquote() << doc.toJson(QJsonDocument::Indented);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 4. QNetworkAccessManager를 사용하여 POST 요청을 보냅니다.
    //    manager는 멤버 변수로 가지고 있는 것이 좋습니다. (new로 매번 생성하지 않기)
    //    여기서는 간단한 예시로 지역 변수로 생성합니다.
    if (!manager_) {
        qCritical() << "CRITICAL ERROR: manager_ is a nullptr! Aborting.";
        QMessageBox::critical(this, "치명적 오류", "네트워크 관리자 객체가 초기화되지 않았습니다.");
        return;
    }
    // 5. [선택사항] 서버의 응답을 처리하기 위한 connect 문 (권장)
    //    요청이 성공했는지, 실패했는지 사용자에게 알려주면 좋습니다.
    connect
    (
        manager_, 
        &QNetworkAccessManager::finished, 
        this, 
        [this](QNetworkReply* reply) 
        {
            if (reply->error() == QNetworkReply::NoError) 
            {
                // 성공!
                qDebug() << "학습 데이터 전송 성공!";
                QMessageBox::information(this, "성공", "학습 데이터가 서버에 성공적으로 저장되었습니다.");
            }
            else
            {
                // 실패!
                qDebug() << "학습 데이터 전송 실패: " << reply->errorString();
                qDebug() << "서버 응답: " << reply->readAll(); // 서버가 보낸 오류 메시지 확인
                QMessageBox::critical(this, "오류", "서버에 학습 데이터를 저장하는 데 실패했습니다.\n" + reply->errorString());
            }
            
            // 메모리 누수 방지를 위해 reply객체를 삭제합니다.
            reply->deleteLater();
        }
    );

    // 6. 실제로 요청을 보냅니다!
    manager_->post(request, jsonData);
}

void PolicyPage::send_all_text_training_data()
{
    constexpr std::array<const char*, 3> kDocExtensions = { ".hwpx", ".docx", ".xlsx" };

    for (const auto& entry : std::filesystem::recursive_directory_iterator(kDefaultTrainDocsDirPath)) 
    {
        if (!entry.is_regular_file())
            continue;

        const std::filesystem::path& doc_path = entry.path();
        const std::string ext = doc_path.extension().string();

        // 문서 확장자 확인
        if (std::find(kDocExtensions.begin(), kDocExtensions.end(), ext) == kDocExtensions.end())
            continue;

        auto result = load_text_labeling_data(doc_path);
        if (!result) 
        {
            std::cerr << "[경고] " << doc_path << " 로딩 실패: " << result.error().message() << '\n';
        } 
        else 
        {
            std::cout << "[정보] " << doc_path << " 로딩 성공: " << result->size() << "개 라벨\n";
            send_text_training_data(result.value());
            // 필요한 후처리 가능
        }
    }
}