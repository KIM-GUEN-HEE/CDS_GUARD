#include "DocumentLabelingDialog.hpp"
#include <QMouseEvent>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QJsonObject>
#include <span>
#include <ranges>
#include <QBuffer>
#include <QGuiApplication>
#include <QGraphicsDropShadowEffect>
#include <QMenu>
#include "FilteringInterface.hpp"

LabelingTextEdit::LabelingTextEdit(DocumentLabelingDialog *_parent) : QTextEdit(_parent)
{
    parent_dialog_ = _parent;
    setReadOnly(false);
    setTextInteractionFlags(Qt::TextSelectableByMouse);
}

void LabelingTextEdit::mouseReleaseEvent(QMouseEvent *event)
{
    QTextEdit::mouseReleaseEvent(event);
}

void LabelingTextEdit::contextMenuEvent(QContextMenuEvent *event)
{
    if (!textCursor().hasSelection()) 
    {
        QTextEdit::contextMenuEvent(event);
        return;
    }

    QMenu menu(this);

    for (const QString& label : parent_dialog_->labeling_options_) 
    {
        QAction* action = menu.addAction(label);
        connect(action, &QAction::triggered, this, 
            [this, label]() 
            {
                emit selectionMade(textCursor().selectionStart(), textCursor().selectionEnd(), label);
            }
        );
    }

    menu.addSeparator();
    QAction* customAction = menu.addAction("직접 입력...");
    connect(customAction, &QAction::triggered, this, 
        [this]() 
        {
            bool ok;
            QString input = QInputDialog::getText(this, "라벨 입력", "새 라벨을 입력하세요:", QLineEdit::Normal, "", &ok);
            if (ok && !input.isEmpty()) 
            {
                emit selectionMade(textCursor().selectionStart(), textCursor().selectionEnd(), input);
                emit newLabelAdded(input);
            }
        }
    );

    menu.exec(event->globalPos());
}

DocumentLabelingDialog::DocumentLabelingDialog(const std::set<QString>& _labeling_options, ParseZip::RawExtractedDocContent& _raw_doc_content, QWidget *_parent)
: QDialog(_parent), tab_widget_(new QTabWidget(this)), labeling_options_(_labeling_options)
{
    setup_dialog_ui();

    size_t xml_string_curr_pos = 0;
    size_t xml_text_start_tag_size = _raw_doc_content.doc_type_info.xml_text_start_tag.size();
    size_t xml_text_end_tag_size = _raw_doc_content.doc_type_info.xml_text_end_tag.size();
    std::vector<QString> q_texts;
    std::vector<QImage> qimage_vec;
    std::vector<QString> qimage_labels;
    std::vector<std::vector<MatchedInfo>> highlights;

    FilteringInterface &filtering_interface = FilteringInterface::instance().get();

    auto processMatchedTextSpan = [&](std::span<char> span)
    {
        auto matched_info_vec = filtering_interface.get_string_matched_info_by_rule(span);

        if (matched_info_vec.empty() || matched_info_vec.back().size_ != span.size())
        {
            q_texts.emplace_back();
            highlights.emplace_back();
            size_t curr_text_pos = 0;

            for (const auto &matched_info : matched_info_vec)
            {
                q_texts.back() += QString::fromUtf8(span.data() + curr_text_pos, matched_info.index_ - curr_text_pos);
                size_t start_index = q_texts.back().size();
                q_texts.back() += QString::fromUtf8(span.data() + matched_info.index_, matched_info.size_);
                size_t end_index = q_texts.back().size();

                highlights.back().emplace_back(start_index, end_index - start_index);
                curr_text_pos = matched_info.index_ + matched_info.size_;
            }

            q_texts.back() += QString::fromUtf8(span.data() + curr_text_pos, span.size() - curr_text_pos);
        }
    };

    while ((xml_string_curr_pos = _raw_doc_content.xml_text.find(_raw_doc_content.doc_type_info.xml_text_start_tag, xml_string_curr_pos)) != std::string::npos) 
    {
        size_t xml_string_next_pos = _raw_doc_content.xml_text.find(_raw_doc_content.doc_type_info.xml_text_end_tag, xml_string_curr_pos);
        if (xml_string_next_pos == std::string::npos) break;

        size_t text_start = xml_string_curr_pos + xml_text_start_tag_size;
        size_t text_end = xml_string_next_pos;

        if (_raw_doc_content.doc_type == ParseZip::DocType::kHwpx)
        {
            size_t pos = text_start;
            while (pos < text_end)
            {
                size_t hp_pos = _raw_doc_content.xml_text.find("<hp:", pos);
                if (hp_pos == std::string::npos || hp_pos >= text_end)
                {
                    std::span<char> span(_raw_doc_content.xml_text.data() + pos, text_end - pos);
                    processMatchedTextSpan(span);  // 아래에서 정의
                    break;
                }

                if (hp_pos > pos)
                {
                    std::span<char> span(_raw_doc_content.xml_text.data() + pos, hp_pos - pos);
                    processMatchedTextSpan(span);
                }

                size_t tag_close = _raw_doc_content.xml_text.find('>', hp_pos);
                if (tag_close == std::string::npos || tag_close >= text_end)
                    break;

                pos = tag_close + 1;  // 태그 건너뛰기
            }
        }
        else
        {
            std::span<char> span(_raw_doc_content.xml_text.data() + text_start, text_end - text_start);
            processMatchedTextSpan(span);
        }

        xml_string_curr_pos = xml_string_next_pos + xml_text_end_tag_size;
    }

    const auto matToQImage = [](const cv::Mat &mat) -> QImage
    {
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888).rgbSwapped();
    };

    qimage_vec.reserve(_raw_doc_content.images.size());
    for (const auto& [ offset, image ] : _raw_doc_content.images)
    {
        qimage_vec.push_back(matToQImage(image));
        qimage_labels.push_back(QString());
    }

    // 텍스트 탭
    setup_text_tab(q_texts, highlights);

    // 이미지 탭
    setup_image_tab(qimage_vec, qimage_labels);
}

DocumentLabelingDialog::DocumentLabelingDialog(const std::set<QString>& _labeling_options, QJsonArray _text_label_json, QJsonArray _image_label_json, QWidget *_parent)
: QDialog(_parent), tab_widget_(new QTabWidget(this)), labeling_options_(_labeling_options)
{
    setup_dialog_ui();
    load_text_labels_from_json(_text_label_json);
    setup_text_tab();
}

void DocumentLabelingDialog::setup_dialog_ui()
{
    setWindowTitle("라벨링");
    
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);  // 배경 투명
    
    QVBoxLayout* outer_layout = new QVBoxLayout(this);
    outer_layout->setContentsMargins(20, 20, 20, 20); // 그림자 여백
    
    QFrame* rounded_frame = new QFrame;
    rounded_frame->setStyleSheet
    (
        R"(
            QFrame 
            {
                background-color: white;
                border-radius: 12px;
            }
        )"
    );
            
    QVBoxLayout* inner_layout = new QVBoxLayout(rounded_frame);
    
    outer_layout->addWidget(rounded_frame);
    
    auto* shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 0);
    shadow->setColor(QColor(0, 0, 0, 50));
    rounded_frame->setGraphicsEffect(shadow);
            
    inner_layout->addWidget(tab_widget_);

    // 버튼
    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    inner_layout->addWidget(btns);
    
    if (this->parentWidget() != nullptr)
    {
        resize(parentWidget()->size());
        move(parentWidget()->pos());
    }
}

void DocumentLabelingDialog::setup_text_tab_ui()
{
    QWidget *text_tab = new QWidget;
    QVBoxLayout *text_layout = new QVBoxLayout(text_tab);

    QScrollArea *scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    text_container_ = new QWidget;
    text_container_layout_ = new QVBoxLayout(text_container_);
    text_container_layout_->setSpacing(4);
    text_container_layout_->setContentsMargins(4, 4, 4, 4);

    text_container_->setLayout(text_container_layout_);
    scroll->setWidget(text_container_);
    scroll->setWidgetResizable(true);
    text_layout->addWidget(scroll);
    tab_widget_->addTab(text_tab, "텍스트 라벨링");
}

LabelingTextEdit* DocumentLabelingDialog::make_new_edit(const QString &text)
{
    LabelingTextEdit *edit = new LabelingTextEdit(this);
    edit->setPlainText(text);
    
    edit->setStyleSheet
    (
        R"(
            QTextEdit 
            {
                border: 1px solid #ccc;
                border-radius: 0px;
                padding: 4px;
                font-size: 13px;
            }
        )"
    );
    edit->setLineWrapMode(QTextEdit::WidgetWidth);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    edit->document()->setTextWidth(edit->viewport()->width());
    edit->document()->adjustSize();
    
    QSizeF docSize = edit->document()->size();
    int newHeight = static_cast<int>(docSize.height()) + 8;
    edit->setFixedHeight(newHeight);
    
    connect(edit, &LabelingTextEdit::selectionMade, this, &DocumentLabelingDialog::handle_selection);
    connect(edit, &LabelingTextEdit::newLabelAdded, this, [this](const QString& label) { labeling_options_.insert(label); });
    
    return edit;
}
        
LabelingTextEdit* DocumentLabelingDialog::make_new_edit(const QString &text, const std::vector<MatchedInfo>& highlight_info)
{
    sentences_.push_back(Sentence{ text });
    LabelingTextEdit *edit = make_new_edit(text);
    // 자동 하이라이팅 적용
    for (const auto& mi : highlight_info)
    {
        QTextCursor cursor = edit->textCursor();
        cursor.setPosition(mi.index_);
        cursor.setPosition(mi.index_ + mi.size_, QTextCursor::KeepAnchor);
        
        QTextCharFormat fmt;
        fmt.setBackground(Qt::lightGray);
        cursor.setCharFormat(fmt);
        fmt.setToolTip("SECRET");
        sentences_.back().labels.push_back({ mi.index_, mi.index_ + mi.size_, QStringLiteral("SECRET") });
    }
            
    edit->setSentenceIndex(sentences_.size() - 1);
    return edit;
}

LabelingTextEdit* DocumentLabelingDialog::make_new_edit(size_t sentence_index)
{
    LabelingTextEdit *edit = make_new_edit(sentences_[sentence_index].text);
    edit->setSentenceIndex(sentence_index);

    for (const auto& label : sentences_[sentence_index].labels)
    {
        QTextCursor cursor = edit->textCursor();
        cursor.setPosition(label.start);
        cursor.setPosition(label.end, QTextCursor::KeepAnchor);

        QTextCharFormat fmt;
        fmt.setBackground(Qt::yellow);
        fmt.setToolTip(label.type);
        cursor.setCharFormat(fmt);
    }

    return edit;
}

void DocumentLabelingDialog::handle_selection(size_t start, size_t end, const QString& selected)
{
    auto* edit = qobject_cast<LabelingTextEdit*>(sender());
    if (!edit) return;

    size_t i = edit->sentenceIndex();
    if (i >= static_cast<size_t>(sentences_.size()))
        return;

    QString label = selected;

    if (!labeling_options_.contains(label)) {
        bool ok;
        label = QInputDialog::getText(this, "라벨 입력", "라벨을 입력하세요:", QLineEdit::Normal, label, &ok);
        if (!ok || label.isEmpty())
            return;
    }

    QTextCursor cursor = edit->textCursor();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);

    QTextCharFormat fmt;
    fmt.setBackground(Qt::yellow);
    fmt.setToolTip(label);
    cursor.setCharFormat(fmt);

    sentences_[i].labels.push_back({ start, end, label });
}

void DocumentLabelingDialog::setup_text_tab(const std::vector<QString>& q_texts, const std::vector<std::vector<MatchedInfo>>& highlights)
{
    setup_text_tab_ui();

    for (int i = 0; i < static_cast<int>(q_texts.size()); ++i)
    {
        const QString& sentence_text = q_texts[i];
        const auto& highlight_info = highlights[i];

        LabelingTextEdit *edit = make_new_edit(sentence_text, highlight_info);
        text_container_layout_->addWidget(edit);
    }
}

void DocumentLabelingDialog::setup_text_tab()
{
    setup_text_tab_ui();

    for (int i = 0; i < sentences_.size(); i++)
    {
        LabelingTextEdit* edit = make_new_edit(i);
        text_container_layout_->addWidget(edit);
    }
}

QJsonArray DocumentLabelingDialog::get_text_labels() const
{
    QJsonArray result;

    for (const auto& sentence : sentences_) {
        if (sentence.labels.empty())
            continue;

        QJsonObject obj;
        obj["full_text"] = sentence.text;

        QJsonArray annotations;
        for (const auto& label : sentence.labels) {
            QJsonObject ann;
            ann["start"] = static_cast<qint64>(label.start);
            ann["end"] = static_cast<qint64>(label.end);
            ann["label"] = label.type;
            annotations.append(ann);
        }

        obj["annotations"] = annotations;
        result.append(obj);
    }

    return result;
}

QJsonArray DocumentLabelingDialog::get_text_labels_with_none() const
{
    QJsonArray result;

    for (const auto& sentence : sentences_) 
    {
        QJsonObject obj;
        obj["full_text"] = sentence.text;

        QJsonArray annotations;

        if (sentence.labels.empty()) 
        {
            // 라벨이 없는 경우 → 문장 전체 범위에 "NONE" 라벨 부여
            QJsonObject ann;
            ann["start"] = 0;
            ann["end"] = static_cast<qint64>(sentence.text.size());
            ann["label"] = "NONE";
            annotations.append(ann);
        } 
        else 
        {
            // 기존 라벨 추가
            for (const auto& label : sentence.labels) 
            {
                QJsonObject ann;
                ann["start"] = static_cast<qint64>(label.start);
                ann["end"] = static_cast<qint64>(label.end);
                ann["label"] = label.type;
                annotations.append(ann);
            }
        }

        obj["annotations"] = annotations;
        result.append(obj);
    }

    return result;
}

std::vector<DocumentLabelingDialog::LabeledImageJsonWithData> DocumentLabelingDialog::get_image_labels() const
{
    std::vector<LabeledImageJsonWithData> result;
    result.reserve(images_.size()); // 최적화

    for (size_t i = 0; i < images_.size(); ++i) 
    {
        const auto& img = images_[i];

        // 1. QImage → PNG → QByteArray
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        img.image.save(&buffer, "PNG");

        // 2. QByteArray → std::vector<uint8_t>
        std::vector<uint8_t> image_data(byteArray.begin(), byteArray.end());

        // 3. 라벨 JSON 객체 생성
        QJsonObject label_json;
        label_json["index"] = static_cast<int>(i);
        label_json["label"] = img.label;

        // 4. 구조체에 담기
        LabeledImageJsonWithData entry;
        entry.image_data = std::move(image_data);
        entry.label_data = std::move(label_json);

        result.push_back(std::move(entry));
    }

    return result;
}

void DocumentLabelingDialog::load_text_labels_from_json(const QJsonArray& _text_label_json_array)
{
    for (const QJsonValue& val : _text_label_json_array) 
    {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        Sentence sentence;
        sentence.text = obj.value("full_text").toString();

        QJsonArray annotations = obj.value("annotations").toArray();

        for (const QJsonValue& ann_val : annotations) 
        {
            if (!ann_val.isObject()) continue;
            QJsonObject ann = ann_val.toObject();

            QString label_type = ann.value("label").toString();
            if (label_type == "NONE")
                continue;  // "NONE" 라벨은 내부 데이터에 저장하지 않음

            TextLabel label;
            label.start = static_cast<size_t>(ann.value("start").toInteger());
            label.end = static_cast<size_t>(ann.value("end").toInteger());
            label.type = label_type;

            sentence.labels.push_back(std::move(label));
        }

        sentences_.push_back(std::move(sentence));
    }
}