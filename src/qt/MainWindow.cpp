#include "MainWindow.h"
#include <QScrollArea>
#include <QGroupBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    setWindowTitle("FontMaster - Emoji Font Editor");
    setMinimumSize(1000, 700);
    
    statusLabel->setText("Ready - No font loaded");
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
    fontInfoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 5px; border: 1px solid #ccc; }");
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
    
    glyphPreview = new QListWidget(this);
    rightLayout->addWidget(glyphPreview, 1);
    
    statusLabel = new QLabel(this);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("QLabel { background-color: #e0e0e0; padding: 3px; }");
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
                QMessageBox::information(this, "Success", 
                    "Font saved successfully to:\n" + filepath);
            } else {
                QMessageBox::warning(this, "Error", "Failed to save font");
            }
        } catch (const std::exception& e) {
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
                default: formatStr = "Unknown"; break;
            }
            
            auto glyphs = currentFont->listGlyphs();
            fontInfoLabel->setText(
                QString("File: %1\nFormat: %2\nGlyphs: %3")
                    .arg(QFileInfo(filepath).fileName())
                    .arg(formatStr)
                    .arg(glyphs.size())
            );
            
            statusLabel->setText("Font loaded successfully");
            btnSave->setEnabled(true);
            
        } else {
            QMessageBox::warning(this, "Error", "Cannot load font: " + filepath);
        }
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Error", 
            QString("Error loading font:\n") + e.what());
    }
}

void MainWindow::refreshGlyphList() {
    glyphTree->clear();
    
    if (!currentFont) return;
    
    try {
        auto glyphs = currentFont->listGlyphs();
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
        statusLabel->setText(QString("Loaded %1 glyphs").arg(glyphs.size()));
        
    } catch (const std::exception& e) {
        statusLabel->setText("Error loading glyph list: " + QString(e.what()));
    }
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
                statusLabel->setText("Glyph removed: " + glyphName);
                btnSave->setEnabled(true);
            } else {
                QMessageBox::warning(this, "Error", "Failed to remove glyph");
            }
        } catch (const std::exception& e) {
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
        "Select Replacement Image", "", "Image Files (*.png *.jpg *.bmp)");
    
    if (!imagePath.isEmpty()) {
        try {
            QFile file(imagePath);
            if (!file.open(QIODevice::ReadOnly)) {
                QMessageBox::warning(this, "Error", "Cannot open image file");
                return;
            }
            
            QByteArray imageData = file.readAll();
            std::vector<uint8_t> imageVec(imageData.begin(), imageData.end());
            
            if (currentFont->replaceGlyphImage(glyphName.toStdString(), imageVec)) {
                statusLabel->setText("Image replaced for: " + glyphName);
                btnSave->setEnabled(true);
            } else {
                QMessageBox::warning(this, "Error", "Failed to replace glyph image");
            }
        } catch (const std::exception& e) {
            QMessageBox::warning(this, "Error", 
                QString("Failed to replace image:\n") + e.what());
        }
    }
}

void MainWindow::glyphSelectionChanged() {
    auto selectedItems = glyphTree->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();
    
    btnRemove->setEnabled(hasSelection);
    btnReplace->setEnabled(hasSelection);
    
    updateGlyphPreview();
}

void MainWindow::updateGlyphPreview() {
    glyphPreview->clear();
    
    auto selectedItems = glyphTree->selectedItems();
    if (selectedItems.isEmpty() || !currentFont) return;
    
    QTreeWidgetItem *item = selectedItems.first();
    QString glyphName = item->text(0);
    
    try {
        auto glyphInfo = currentFont->getGlyphInfo(glyphName.toStdString());
        
        QString previewText = QString("Glyph: %1\nUnicode: U+%2\nFormat: %3\nSize: %4 bytes")
            .arg(glyphName)
            .arg(glyphInfo.unicode, 4, 16, QChar('0')).toUpper()
            .arg(QString::fromStdString(glyphInfo.format))
            .arg(glyphInfo.data_size);
        
        glyphPreview->addItem(previewText);
        
        // Здесь можно добавить отображение самого изображения глифа
        // Для этого нужно конвертировать image_data в QPixmap
        
    } catch (const std::exception& e) {
        glyphPreview->addItem("Error loading glyph details: " + QString(e.what()));
    }
}

void MainWindow::clearFont() {
    currentFont.reset();
    glyphTree->clear();
    glyphPreview->clear();
    fontInfoLabel->setText("No font loaded");
    statusLabel->setText("Ready - No font loaded");
    btnSave->setEnabled(false);
    btnRemove->setEnabled(false);
    btnReplace->setEnabled(false);
}
