#pragma once

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QTabWidget>
#include <QLabel>
#include <string>
#include <set>
#include <unordered_set>
#include <memory>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <future>
#include <filesystem>
#include <array>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <expected>
#include <QSpinBox>
#include "opencv2/opencv.hpp"
#include "DoubleArrayTrie.hpp"
#include "FilteringInterface.hpp"
#include "CdsGuardOs.hpp"

class PolicyPage : public QWidget 
{
    Q_OBJECT

public:
    explicit PolicyPage(QWidget* _parent = nullptr);
private slots:
    void search_button_clicked();
    void add_button_clicked();
    void prohibit_word_list_right_clicked(const QPoint& _pos) ;
    void prohibited_word_remove_button_clicked();
    void prohibited_word_edit_button_clicked();
    void undo_button_clicked();
    void build_button_clicked();
    void load_button_clicked();
    void regex_apply_button_clicked();
    void open_image_dir_button_clicked() const;
    void update_censor_image_button_clicked();
    void deep_learning_docs_list_refresh_button_clicked();
    void deep_learning_docs_list_right_clicked(const QPoint& _pos);
    void deep_learning_docs_remove_clicked();
    void deep_learning_docs_labeling_clicked();
    void deep_learning_docs_dir_open_button_clicked() const;
    void deep_learning_active_button_clicked();
    void deep_learning_inactive_button_clicked();
    void deep_learning_server_endpoint_set_button_clicked();
    void deep_learning_train_data_send_button_clicked();
    
private:
    void refresh_prohibited_word_list_widget();
    void update_page_buttons();
    void update_total_count_label();
    void mark_pattern_modified();
    void apply_filter(const QString& _keyword);
    void load_patterns_from_file(const std::filesystem::path& _file_path);
    void save_patterns_to_file(const std::filesystem::path& _fill_path);
    void save_labels_to_file(const std::filesystem::path& _file_path);
    void edit_list_item(QListWidgetItem* _list_item);
    void update_pattern_set();
    void save_text_labeling_data(const std::filesystem::path& _doc_file_path, const QJsonArray& label_array);
    std::expected<QJsonArray, std::error_code> load_text_labeling_data(std::filesystem::path _json_file_path);
    void load_labels_from_file(const std::filesystem::path& _file_path);
    void send_text_training_data(const QJsonArray& _training_text_data);
    void send_all_text_training_data();
    std::vector<cv::Mat> load_image_dir_to_vec();
    std::vector<FilteringInterface::CdsRegex> get_cds_regex_enum();
    void build_filtering_interface();
    int get_dpi_scaled(const int _px) const;
    QListWidgetItem* make_new_item(const QString& _string);
    QListWidgetItem* make_new_item(std::string_view _string);

    QLineEdit* search_input_, *ip_address_edit_;
    QSpinBox* port_spin_box_;
    QListWidget *prohibited_word_list_widget_, *deep_learning_docs_list_widget_;
    QPushButton *search_button_, *add_button_, *remove_button_, *edit_button_, 
                *build_button_, *undo_button_, *load_file_button_, *open_image_dir_button_,
                *update_censor_image_button_, *deep_learning_active_button_, *deep_learning_inactive_button_, *deep_learning_docs_dir_open_button_,
                *deep_learning_docs_list_refresh_button, *deep_learning_server_endpoint_set_button_, *deep_learning_train_data_send_button_;
    std::array<QCheckBox*, 6> regex_checkbox_arr;
    QWidget* page_control_widget_;
    QLabel* total_count_label_;
    int prohibited_list_current_page_ = 0;
    int prohibited_list_items_per_page_ = 50;
    inline static const std::filesystem::path kDefaultDicPath = CdsGuardOs::kApplicationDataPath / "data/prohibited_word.txt";
    inline static const std::filesystem::path kDefaultRegPath = CdsGuardOs::kApplicationDataPath / "data/selected_regex.json";
    inline static const std::filesystem::path kDefaultImgDirPath = CdsGuardOs::kApplicationDataPath / "data/image";
    inline static const std::filesystem::path kDefaultTrainDocsDirPath = CdsGuardOs::kApplicationDataPath / "data/train docs";
    inline static const std::filesystem::path kDefaultTrainDocsLabelOptionPath = CdsGuardOs::kApplicationDataPath / "data/train docs/label_option.txt";
    bool pattern_modified_flag_ = false;
    std::set<std::string> pattern_set_, new_inserted_pattern_set_;
    std::set<QString> label_set_;
    std::unordered_set<std::string> will_erase_pattern_set_;
    std::unique_ptr<DoubleArrayTrie> double_array_trie_;
    std::atomic_bool is_building_ = false;
    QNetworkAccessManager* manager_;
    QString deep_learning_server_ip_ = "127.0.0.1";
    int deep_learning_server_port_ = 8000;

    friend class FilteringInterface;
};

struct ExtractedDocsContent 
{
    QString full_text; // UI와 문자 인덱스 호환성을 위해 QString 사용
    // 이미지의 고유 식별자(zip 내 경로)와 이미지 데이터를 쌍으로 저장
    std::vector<std::pair<std::string, cv::Mat>> images; 
};