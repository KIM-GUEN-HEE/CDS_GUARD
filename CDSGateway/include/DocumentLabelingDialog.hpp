#pragma once

#include <vector>
#include <QDialog>
#include <QTextEdit>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QList>
#include <QImage>
#include <QPixmap>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include "ParseZip.hpp"
#include "FilteringInterface.hpp"

class DocumentLabelingDialog;

class LabelingTextEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit LabelingTextEdit(DocumentLabelingDialog *_parent = nullptr);
    inline void setSentenceIndex(size_t idx) { sentence_index_ = idx; }
    inline size_t sentenceIndex() const { return sentence_index_; }

signals:
    void selectionMade(int start, int end, const QString &selected);
    void newLabelAdded(const QString& label);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    DocumentLabelingDialog* parent_dialog_;
    size_t sentence_index_ = std::numeric_limits<size_t>::max(); // 문장 인덱스
};

class DocumentLabelingDialog : public QDialog
{
    Q_OBJECT

    friend class LabelingTextEdit;

    struct TextLabel 
    {
        size_t  start;
        size_t  end;
        QString type; // 라벨 종류 (예: "개인정보", "표", "그림", 사용자 정의 문자열)
    };

    struct Sentence 
    {
        QString text;
        std::vector<TextLabel> labels;
    };

    struct LabeledImage 
    {
        QImage image;
        QString label;
    };
    
public:
    struct LabeledImageJsonWithData
    {
        std::vector<uint8_t> image_data;
        QJsonObject label_data;
    };

    DocumentLabelingDialog(const std::set<QString>& _labeling_options, ParseZip::RawExtractedDocContent& _raw_doc_content, QWidget *_parent = nullptr);
    DocumentLabelingDialog(const std::set<QString>& _labeling_options, QJsonArray _text_label_json, QJsonArray _image_label_json = {}, QWidget *_parent = nullptr);

    QJsonArray get_text_labels() const;
    QJsonArray get_text_labels_with_none() const;
    std::vector<LabeledImageJsonWithData> get_image_labels() const;

protected:

private slots:
    void handle_selection(size_t start, size_t end, const QString& selected);

private:
    void setup_text_tab(const std::vector<QString>& q_texts, const std::vector<std::vector<MatchedInfo>>& highlights);
    void setup_text_tab();

    template <std::ranges::input_range ImageRange> requires std::convertible_to<std::ranges::range_value_t<ImageRange>, QImage>
    void setup_image_tab(ImageRange&& images, std::vector<QString>& image_labels);
    void load_text_labels_from_json(const QJsonArray& _text_label_json_array);
    void setup_dialog_ui();
    LabelingTextEdit* make_new_edit();
    LabelingTextEdit* make_new_edit(const QString &text);
    LabelingTextEdit* make_new_edit(const QString &text, const std::vector<MatchedInfo>& highlight_info);
    LabelingTextEdit* make_new_edit(size_t sentence_index);
    void setup_text_tab_ui();

    QTabWidget *tab_widget_;
    QPoint drag_position_;
    std::set<QString> labeling_options_;
    QWidget *text_container_;
    QVBoxLayout *text_container_layout_;
    LabelingTextEdit *text_edit_;
    std::vector<Sentence> sentences_;
    std::vector<LabeledImage> images_;
};

template <std::ranges::input_range ImageRange> requires std::convertible_to<std::ranges::range_value_t<ImageRange>, QImage>
void DocumentLabelingDialog::setup_image_tab(ImageRange&& images, std::vector<QString>& image_labels)
{
    QWidget *image_tab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(image_tab);
    QScrollArea *scroll = new QScrollArea;
    QWidget *container = new QWidget;
    QVBoxLayout *list_layout = new QVBoxLayout(container);

    for (size_t i = 0; i < images.size(); ++i)
    {
        QHBoxLayout *row = new QHBoxLayout;
        QLabel *img_label = new QLabel;
        img_label->setPixmap(QPixmap::fromImage(images[i]).scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        row->addWidget(img_label);

        QComboBox *label_box = new QComboBox;
        label_box->addItems(QList<QString>(labeling_options_.begin(), labeling_options_.end()));
        connect(label_box, &QComboBox::currentTextChanged, this, 
            [this, i](const QString &text) 
            {
                if (i < images_.size()) 
                {
                    images_[i].label = text;
                }
            }
        );

        row->addWidget(label_box);
        list_layout->addLayout(row);
    }

    container->setLayout(list_layout);
    scroll->setWidget(container);
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll);
    tab_widget_->addTab(image_tab, "이미지 라벨링");
}