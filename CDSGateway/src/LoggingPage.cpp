#include <QLayout>
#include <QCheckBox>
#include <QGuiApplication>
#include <QScreen>
#include <QDesktopServices>
#include <QMessageBox>
#include <QTextBlock>
#include "LoggingPage.hpp"

LoggingPage::LoggingPage(QWidget* _parent) 
: kLogFileName_(std::format("{:%Y-%m-%d %H:%M:%S %Z}.log", std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()})), 
  kLogDir_(CdsGuardOs::kApplicationDataPath / "log"), kLogFilePath_(kLogDir_ / kLogFileName_),
  logging_state_flag_(false)
{
    QVBoxLayout* logging_page_layout = new QVBoxLayout(this);
    QHBoxLayout* header_setting_layout = new QHBoxLayout;
    logging_page_layout->addLayout(header_setting_layout);

    int button_padding_v = get_dpi_scaled(4);
    int button_padding_h = get_dpi_scaled(12);
    int button_radius = get_dpi_scaled(3);
    int button_font_size = get_dpi_scaled(12);

    QString logging_control_button_style = QString(R"(
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
    )").arg(button_radius).arg(button_padding_v).arg(button_padding_h).arg(button_font_size);

    logging_start_button_ = new QPushButton("로깅 시작", this);
    logging_start_button_->setStyleSheet(logging_control_button_style);
    logging_start_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    header_setting_layout->addWidget(logging_start_button_);
    
    logging_stop_button_ = new QPushButton("로깅 중지", this);
    logging_stop_button_->setStyleSheet(logging_control_button_style);
    logging_stop_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    header_setting_layout->addWidget(logging_stop_button_);
    
    open_log_dir_button_ = new QPushButton("로그 폴더 열기", this);
    open_log_dir_button_->setStyleSheet(logging_control_button_style);
    open_log_dir_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    header_setting_layout->addWidget(open_log_dir_button_);
    
    connect(logging_start_button_, &QPushButton::clicked, this, &LoggingPage::logging_start_button_clicked);
    connect(logging_stop_button_, &QPushButton::clicked, this, &LoggingPage::logging_stop_button_clicked);
    connect(open_log_dir_button_, &QPushButton::clicked, this, &LoggingPage::open_log_dir_button_clicked);
    
    header_setting_layout->addStretch();
    
    logging_state_label_ = new QLabel(this);
    header_setting_layout->addWidget(logging_state_label_);
    
    log_output_text_edit_ = new QPlainTextEdit(this);
    log_output_text_edit_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    log_output_text_edit_->setReadOnly(true);
    log_output_text_edit_->setFont(QFont("Consolas", get_dpi_scaled(10)));
    logging_page_layout->addWidget(log_output_text_edit_);
    
    if (!std::filesystem::exists(kLogDir_)) std::filesystem::create_directories(kLogDir_);
    {
        std::filesystem::create_directories(kLogDir_);
    }

    FilteringInterface::instance().get().set_log_callback_func(
        [this](std::string_view text, const std::vector<MatchedInfo>& vec) 
        {
            this->append_log(text, vec);
        }
    );
    logging_stop_button_clicked();
    logging_state_label_update();
}

void LoggingPage::logging_start_button_clicked()
{
    log_file_ofstream_.open(kLogFilePath_);

    if (log_file_ofstream_.is_open())
    {
        logging_state_flag_ = true;
        logging_state_label_update();
    }
    else
    {
        QMessageBox* msg_box = new QMessageBox(
            QMessageBox::Critical,
            "로그 파일 쓰기 열기 실패",
            "파일을 열 수 없습니다. 권한을 확인하십시오.",
            QMessageBox::Ok,
            this
        );

        msg_box->setAttribute(Qt::WA_DeleteOnClose);
        msg_box->show();
    }
}

void LoggingPage::logging_stop_button_clicked()
{
    log_file_ofstream_.close();

    if (log_file_ofstream_.is_open() == false)
    {
        logging_state_flag_ = false;
        logging_state_label_update();
    }
    else
    {
        QMessageBox* msg_box = new QMessageBox(
            QMessageBox::Critical,
            "로그 파일 닫기 실패",
            "파일 닫는 중 문제가 발생했습니다.",
            QMessageBox::Ok,
            this
        );

        msg_box->setAttribute(Qt::WA_DeleteOnClose);
        msg_box->show();
    }
}

void LoggingPage::open_log_dir_button_clicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(kLogDir_.c_str()));
}

void LoggingPage::logging_state_label_update()
{
    static int font_size = get_dpi_scaled(14);

    if (logging_state_flag_ == true)
    {
        logging_state_label_->setStyleSheet(std::format("font-size: {}px; font-weight: bold; color: #4a66f1;", font_size).c_str());
        logging_state_label_->setText("로깅 중");
    }
    else
    {
        logging_state_label_->setStyleSheet(std::format("font-size: {}px; font-weight: bold; color: black;", font_size).c_str());
        logging_state_label_->setText("로깅 중지");
    }
}

void LoggingPage::append_log(const std::string_view _censor_text, const std::vector<MatchedInfo>& _matched_info_vec)
{
    if (logging_state_flag_ == true)
    {
        std::string log_entry;
        log_entry += "[CENSORED TEXT] ";
        log_entry += _censor_text;
        log_entry += "\n";

        log_entry += "[MATCHED LOCATIONS] ";
        for (const auto& [index, size] : _matched_info_vec)
        {
            log_entry += '[';
            log_entry += std::to_string(index);
            log_entry += ", ";
            log_entry += std::to_string(size);
            log_entry += "] ";
        }
        log_entry += "\n";
    
        // 2. UI 쓰기는 메인 스레드에서
        QMetaObject::invokeMethod(
            this, 
            [this, log_entry]()
            {
                QTextDocument* doc = log_output_text_edit_->document();
                constexpr int kMaxLines = 1000;

                // 최대 줄 수 제한
                while (doc->blockCount() > kMaxLines)
                {
                    QTextBlock block = doc->firstBlock();
                    QTextCursor cursor(block);
                    cursor.select(QTextCursor::BlockUnderCursor);
                    cursor.removeSelectedText();
                    cursor.deleteChar();
                }

                log_output_text_edit_->appendPlainText(QString::fromUtf8(log_entry));
            }, 
            Qt::QueuedConnection
        );
    
        static std::mutex log_file_mutex;
        std::lock_guard lock(log_file_mutex);
        if (log_file_ofstream_.is_open())
        {
            log_file_ofstream_ << log_entry;
        }
    }
}

int LoggingPage::get_dpi_scaled(int _px) const
{
    static const qreal scale = QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0;
    return int(std::round(_px * scale));
}