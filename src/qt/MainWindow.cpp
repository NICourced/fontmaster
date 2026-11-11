#include "MainWindow.h"
#include "fontmaster/EmojiRenderer.h"
#include <QScrollArea>
#include <QGroupBox>
#include <QPixmap>
#include <QImage>
#include <QIcon>
#include <QLabel>
#include <QPainter>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    setWindowTitle("FontMaster - Emoji Font Editor");
    setMinimumSize(1000, 700);

    setStatus("Ready - No font loaded");
}

void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // Левая панель - список глифов и управление
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);

    // Информация о шрифте
    fontInfoLabel = new QLabel("No font loaded", this);
    fontInfoLabel->setWordWrap(true);
    fontInfoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; color: #1f2937; padding: 5px; border: 1px solid #ccc; }");
    leftLayout->addWidget(fontInfoLabel);

    // Панель управления
    QHBoxLayout *controlLayout = new QHBoxLayout();
    btnOpen = new QPushButton("Open Font", this);
    btnSave = new QPushButton("Save Font", this);
    btnRemove = new QPushButton("Remove Glyph", this);
    btnReplace = new QPushButton("Replace Image", this);

    btnSave->setEnabled(false);
    btnRemove->setEnabled(false);
    btnReplace->setEnabled(false);

    controlLayout->addWidget(btnOpen);
    controlLayout->addWidget(btnSave);
    controlLayout->addWidget(btnRemove);
    controlLayout->addWidget(btnReplace);
    leftLayout->addLayout(controlLayout);

    // Список глифов
    glyphTree = new QTreeWidget(this);
    glyphTree->setHeaderLabels({"Glyph Name", "Unicode", "Format", "Size"});
    glyphTree->setSortingEnabled(true);
    glyphTree->setSelectionMode(QTreeWidget::SingleSelection);
    leftLayout->addWidget(glyphTree, 1);

    // Правая панель - превью
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);

    QLabel *previewLabel = new QLabel("Glyph Preview", this);
    previewLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(previewLabel);

    auto* glyphPreview = new QScrollArea(this);
    glyphPreview->setWidgetResizable(true);
    auto* previewContent = new QWidget();
    auto* previewLayout = new QVBoxLayout(previewContent);
    previewLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    previewTextLabel = new QLabel("Select a glyph to preview", previewContent);
    previewTextLabel->setTextFormat(Qt::PlainText);
    previewTextLabel->setWordWrap(true);
    previewLayout->addWidget(previewTextLabel);
    previewImageLabel = new QLabel(previewContent);
    previewImageLabel->setFixedSize(256, 256);
    previewImageLabel->setAlignment(Qt::AlignCenter);
    previewImageLabel->hide();
    previewLayout->addWidget(previewImageLabel, 0, Qt::AlignHCenter);
    previewLayout->addStretch();
    glyphPreview->setWidget(previewContent);
    rightLayout->addWidget(glyphPreview, 1);

    statusLabel = new QLabel(this);
    statusLabel->setTextFormat(Qt::PlainText);
    statusLabel->setWordWrap(true);
    statusLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(statusLabel);

    mainLayout->addWidget(leftPanel, 2);
    mainLayout->addWidget(rightPanel, 1);

    // Меню
    QMenu *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Open Font", this, &MainWindow::openFont);
    fileMenu->addAction("Save Font", this, &MainWindow::saveFont);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    // Соединения
    connect(btnOpen, &QPushButton::clicked, this, &MainWindow::openFont);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::saveFont);
    connect(btnRemove, &QPushButton::clicked, this, &MainWindow::removeSelectedGlyph);
    connect(btnReplace, &QPushButton::clicked, this, &MainWindow::replaceGlyphImage);
    connect(glyphTree, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::glyphSelectionChanged);
}

void MainWindow::setStatus(const QString& message, Status status) {
    const char* background = "#e0e0e0";
    const char* foreground = "#1f2937";
    if (status == Status::Success) {
        background = "#166534";
        foreground = "#ffffff";
    } else if (status == Status::Error) {
        background = "#b91c1c";
        foreground = "#ffffff";
    }
    statusLabel->setStyleSheet(
        QString("QLabel { background-color: %1; color: %2; padding: 6px; }")
            .arg(background).arg(foreground));
    statusLabel->setText(message);
}

void MainWindow::openFont() {
    QString filepath = QFileDialog::getOpenFileName(this, "Open Font File",
                                                   "", "Font Files (*.ttf *.otf)");
    if (!filepath.isEmpty()) {
        loadFont(filepath);
    }
}

void MainWindow::saveFont() {
    if (!currentFont) return;

    QString filepath = QFileDialog::getSaveFileName(this, "Save Font File",
                                                   currentFontPath + ".modified.ttf",
                                                   "Font Files (*.ttf)");
    if (!filepath.isEmpty()) {
        try {
            if (currentFont->save(filepath.toStdString())) {
                setStatus("Font saved successfully", Status::Success);
                QMessageBox::information(this, "Success",
                    "Font saved successfully to:\n" + filepath);
            } else {
                setStatus("Failed to save font", Status::Error);
                QMessageBox::warning(this, "Error", "Failed to save font");
            }
        } catch (const std::exception& e) {
            setStatus("Failed to save font: " + QString(e.what()), Status::Error);
            QMessageBox::warning(this, "Error",
                QString("Failed to save font:\n") + e.what());
        }
    }
}

void MainWindow::loadFont(const QString& filepath) {
    try {
        clearFont();

        currentFont = fontmaster::Font::load(filepath.toStdString());
        if (currentFont) {
            currentFontPath = filepath;
            refreshGlyphList();

            // Обновляем информацию о шрифте
            QString formatStr;
            switch (currentFont->getFormat()) {
                case fontmaster::FontFormat::CBDT_CBLC: formatStr = "Google CBDT/CBLC"; break;
                case fontmaster::FontFormat::SBIX: formatStr = "Apple SBIX"; break;
                case fontmaster::FontFormat::COLR_CPAL: formatStr = "Microsoft COLR/CPAL"; break;
                case fontmaster::FontFormat::SVG: formatStr = "Adobe SVG"; break;
                case fontmaster::FontFormat::STANDARD: formatStr = "OpenType outline font"; break;
                case fontmaster::FontFormat::NOTO_BREEZE_ADWAITA_NERD: formatStr = "OpenType outline font"; break;
                default: formatStr = "Unknown"; break;
            }

            fontInfoLabel->setText(
                QString("File: %1\nFormat: %2\nGlyphs: %3")
                    .arg(QFileInfo(filepath).fileName())
                    .arg(formatStr)
                    .arg(glyphTree->topLevelItemCount())
            );

            setStatus("Font loaded successfully", Status::Success);
            btnSave->setEnabled(true);

        } else {
            setStatus("Cannot load font: " + filepath, Status::Error);
            QMessageBox::warning(this, "Error", "Cannot load font: " + filepath);
        }
    } catch (const std::exception& e) {
        clearFont();
        setStatus("Cannot load font: " + QString(e.what()), Status::Error);
        QMessageBox::warning(this, "Error",
            QString("Error loading font:\n") + e.what());
    }
}

void MainWindow::refreshGlyphList() {
    glyphTree->clear();

    if (!currentFont) return;

    auto glyphs = currentFont->listGlyphMetadata();
    for (const auto& glyph : glyphs) {
        QTreeWidgetItem *item = new QTreeWidgetItem(glyphTree);
        item->setText(0, QString::fromStdString(glyph.name));

        if (glyph.unicode != 0) {
            item->setText(1, QString("U+%1").arg(glyph.unicode, 4, 16, QChar('0')).toUpper());
        }

        item->setText(2, QString::fromStdString(glyph.format));
        item->setText(3, QString("%1 bytes").arg(glyph.data_size));
    }

    glyphTree->header()->resizeSections(QHeaderView::ResizeToContents);
}

void MainWindow::removeSelectedGlyph() {
    auto selectedItems = glyphTree->selectedItems();
    if (selectedItems.isEmpty() || !currentFont) return;

    QTreeWidgetItem *item = selectedItems.first();
    QString glyphName = item->text(0);

    int result = QMessageBox::question(this, "Remove Glyph",
        "Are you sure you want to remove glyph:\n" + glyphName + "?");

    if (result == QMessageBox::Yes) {
        try {
            if (currentFont->removeGlyph(glyphName.toStdString())) {
                delete item;
                setStatus("Glyph removed: " + glyphName, Status::Success);
                btnSave->setEnabled(true);
            } else {
                setStatus("Failed to remove glyph: " + glyphName, Status::Error);
                QMessageBox::warning(this, "Error", "Failed to remove glyph");
            }
        } catch (const std::exception& e) {
            setStatus("Failed to remove glyph: " + QString(e.what()), Status::Error);
            QMessageBox::warning(this, "Error",
                QString("Failed to remove glyph:\n") + e.what());
        }
    }
}

void MainWindow::replaceGlyphImage() {
    auto selectedItems = glyphTree->selectedItems();
    if (selectedItems.isEmpty() || !currentFont) return;

    QTreeWidgetItem *item = selectedItems.first();
    QString glyphName = item->text(0);

    QString imagePath = QFileDialog::getOpenFileName(this,
        "Select Replacement Image", "",
        (currentFont->getFormat() == fontmaster::FontFormat::CBDT_CBLC || currentFont->getFormat() == fontmaster::FontFormat::SBIX)
            ? "PNG Images (*.png)" : "Artwork (*.png *.svg *.svgz)");

    if (!imagePath.isEmpty()) {
        try {
            QFile file(imagePath);
            if (!file.open(QIODevice::ReadOnly)) {
                setStatus("Cannot open image file", Status::Error);
                QMessageBox::warning(this, "Error", "Cannot open image file");
                return;
            }

            QByteArray imageData = file.readAll();
            std::vector<uint8_t> imageVec(imageData.begin(), imageData.end());

            if (currentFont->replaceGlyphImage(glyphName.toStdString(), imageVec)) {
                const auto updated = currentFont->getGlyphInfo(glyphName.toStdString());
                item->setText(2, QString::fromStdString(updated.format));
                item->setText(3, QString("%1 bytes").arg(updated.data_size));
                updateGlyphPreview();
                setStatus("Image replaced for: " + glyphName, Status::Success);
                btnSave->setEnabled(true);
            } else {
                setStatus("Failed to replace glyph image", Status::Error);
                QMessageBox::warning(this, "Error", "Failed to replace glyph image");
            }
        } catch (const std::exception& e) {
            setStatus("Failed to replace image: " + QString(e.what()), Status::Error);
            QMessageBox::warning(this, "Error",
                QString("Failed to replace image:\n") + e.what());
        }
    }
}

void MainWindow::glyphSelectionChanged() {
    auto selectedItems = glyphTree->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();

    btnRemove->setEnabled(hasSelection);
    btnReplace->setEnabled(hasSelection && currentFont);

    updateGlyphPreview();
}

void MainWindow::updateGlyphPreview() {
    previewTextLabel->setText("Select a glyph to preview");
    previewImageLabel->clear();
    previewImageLabel->hide();

    auto selectedItems = glyphTree->selectedItems();
    if (selectedItems.isEmpty() || !currentFont) return;

    QTreeWidgetItem *item = selectedItems.first();
    QString glyphName = item->text(0);

    try {
        auto glyphInfo = currentFont->getGlyphInfo(glyphName.toStdString());

        QString previewText = QString("Glyph: %1\nUnicode: U+%2\nFormat: %3\nSize: %4 bytes")
            .arg(glyphName)
            .arg(QString::number(glyphInfo.unicode, 16).rightJustified(4, '0').toUpper())
            .arg(QString::fromStdString(glyphInfo.format))
            .arg(glyphInfo.data_size);

        const auto png = fontmaster::renderGlyph(*currentFont, glyphName.toStdString(), 256);
        QPixmap pixmap;
        if (!pixmap.loadFromData(png.data(), static_cast<uint>(png.size()), "PNG"))
            throw fontmaster::FontException("Cannot decode rendered glyph preview");
        previewTextLabel->setText(previewText);
        previewImageLabel->setPixmap(pixmap);
        previewImageLabel->show();
        setStatus("Glyph preview: " + glyphName, Status::Success);

    } catch (const std::exception& e) {
        const QString message = "Error loading glyph details: " + QString(e.what());
        previewTextLabel->setText(message);
        setStatus(message, Status::Error);
    }
}

void MainWindow::clearFont() {
    currentFont.reset();
    currentFontPath.clear();
    glyphTree->clear();
    updateGlyphPreview();
    fontInfoLabel->setText("No font loaded");
    setStatus("Ready - No font loaded");
    btnSave->setEnabled(false);
    btnRemove->setEnabled(false);
    btnReplace->setEnabled(false);
}
