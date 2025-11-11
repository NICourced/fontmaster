#pragma once

#include <QMainWindow>
#include <QTreeWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include "fontmaster/FontMaster.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void openFont();
    void saveFont();
    void removeSelectedGlyph();
    void replaceGlyphImage();
    void glyphSelectionChanged();
    void updateGlyphPreview();

private:
    void setupUI();
    void loadFont(const QString& filepath);
    void refreshGlyphList();
    void clearFont();
    
    QTreeWidget *glyphTree;
    QListWidget *glyphPreview;
    QPushButton *btnOpen;
    QPushButton *btnSave;
    QPushButton *btnRemove;
    QPushButton *btnReplace;
    QLabel *statusLabel;
    QLabel *fontInfoLabel;
    
    std::unique_ptr<fontmaster::Font> currentFont;
    QString currentFontPath;
};
