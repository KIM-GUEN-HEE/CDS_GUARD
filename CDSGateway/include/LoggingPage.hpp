#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <chrono>
#include <fstream>
#include "CdsGuardOs.hpp"
#include "DoubleArrayTrie.hpp"
#include "FilteringInterface.hpp"

class LoggingPage : public QWidget 
{
    Q_OBJECT
public:
    explicit LoggingPage(QWidget* _parent = nullptr);

private slots:
    void logging_start_button_clicked();
    void logging_stop_button_clicked();
    void open_log_dir_button_clicked();

private:
    int get_dpi_scaled(const int _px) const;
    void logging_state_label_update();
    void append_log(const std::string_view _censor_text, const std::vector<MatchedInfo>& _matched_info_vec);
    QPushButton *logging_start_button_, *logging_stop_button_, *open_log_dir_button_;
    QLabel* logging_state_label_;
    QPlainTextEdit* log_output_text_edit_;
    const std::string kLogFileName_;
    const std::filesystem::path kLogDir_ = CdsGuardOs::kApplicationDataPath / "log";
    const std::filesystem::path kLogFilePath_ = kLogDir_ / kLogFileName_;
    std::ofstream log_file_ofstream_;
    bool logging_state_flag_;
};