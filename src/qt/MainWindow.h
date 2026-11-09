#pragma once

#include <QMainWindow>
#include <QTreeWidget>
#include <QScrollArea>
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
    friend struct MainWindowTest;

    enum class Status { Neutral, Success, Error };
    void setupUI();
    void loadFont(const QString& filepath);
    void refreshGlyphList();
    void clearFont();
    void setStatus(const QString& message, Status status = Status::Neutral);

    QTreeWidget *glyphTree;
    QLabel *previewTextLabel;
    QLabel *previewImageLabel;
    QPushButton *btnOpen;
    QPushButton *btnSave;
    QPushButton *btnRemove;
    QPushButton *btnReplace;
    QLabel *statusLabel;
    QLabel *fontInfoLabel;

    std::unique_ptr<fontmaster::Font> currentFont;
    QString currentFontPath;
};
