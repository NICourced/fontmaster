#include "MainWindow.h"
#include <QApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void settle() {
    QEventLoop loop;
    QTimer::singleShot(150, &loop, &QEventLoop::quit);
    loop.exec();
}

QLabel* previewImage(MainWindow& window) {
    for (auto* label : window.findChildren<QLabel*>()) {
        if (!label->pixmap(Qt::ReturnByValue).isNull()) return label;
    }
    throw std::runtime_error("No rendered glyph image in the preview");
}

void checkVisiblePreview(MainWindow& window) {
    settle();
    auto* image = previewImage(window);
    require(image->pixmap(Qt::ReturnByValue).size() == QSize(256, 256),
            "Preview lost its rendered image");
    require(image->visibleRegion().boundingRect().height() >= 256,
            "Preview is clipped after Qt processes its layout events");
}

void checkStatus(QLabel* label, const QColor& background) {
    require(label->palette().color(QPalette::WindowText) == QColor(Qt::white),
            "Status text must be white");
    require(label->palette().color(QPalette::Window) == background,
            "Status background does not match its outcome");
}
}

struct MainWindowTest {
    static void loadFont(const QString& path) {
        MainWindow window;
        window.show();
        window.loadFont(path);
        require(window.currentFont != nullptr, "Font failed to load into GUI");
        require(window.glyphTree->topLevelItemCount() > 0, "GUI glyph list is empty");
        settle();
        checkStatus(window.statusLabel, QColor("#166534"));
        const auto name = window.currentFont->findGlyphName(0x1F600);
        const auto matches = window.glyphTree->findItems(
            QString::fromStdString(name), Qt::MatchExactly, 0);
        require(matches.size() == 1, "Cannot find smiling glyph in GUI");
        window.glyphTree->setCurrentItem(matches.first());
        checkVisiblePreview(window);
        std::cout << "Loaded " << window.glyphTree->topLevelItemCount()
                  << " glyphs and rendered a preview\n";
    }

    static void run(const QString& fontDirectory) {
        MainWindow window;
        window.show();
        for (const auto& name : {"NotoColorEmoji.ttf", "Noto-COLRv1.ttf"}) {
            window.loadFont(fontDirectory + '/' + name);
            require(window.currentFont != nullptr, "Font failed to load");
            settle();
            const auto glyph = window.currentFont->findGlyphName(0x1F600);
            const auto matches = window.glyphTree->findItems(
                QString::fromStdString(glyph), Qt::MatchExactly, 0);
            require(matches.size() == 1, "Cannot find the preview test glyph");
            window.glyphTree->setCurrentItem(matches.first());
            checkVisiblePreview(window);
            checkStatus(window.statusLabel, QColor("#166534"));
            window.resize(1200, 800);
            checkVisiblePreview(window);
            window.resize(1000, 700);
            window.hide();
            window.show();
            checkVisiblePreview(window);
            window.glyphTree->clearSelection();
            settle();
            for (auto* label : window.findChildren<QLabel*>()) {
                require(label->pixmap(Qt::ReturnByValue).isNull(),
                        "Deselection left a stale preview image");
            }
            window.glyphTree->setCurrentItem(matches.first());
            checkVisiblePreview(window);
        }

        QTemporaryDir missingFont;
        QTimer closeWarning;
        QObject::connect(&closeWarning, &QTimer::timeout, [] {
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (auto* message = qobject_cast<QMessageBox*>(widget)) message->accept();
            }
        });
        closeWarning.start(10);
        window.loadFont(missingFont.path() + "/missing.ttf");
        closeWarning.stop();
        settle();
        require(!window.currentFont, "Failed load left a stale font");
        require(!window.btnSave->isEnabled() && !window.btnRemove->isEnabled() &&
                !window.btnReplace->isEnabled(), "Failed load left editing enabled");
        require(window.previewImageLabel->pixmap(Qt::ReturnByValue).isNull(),
                "Failed load left a stale preview image");
        checkStatus(window.statusLabel, QColor("#b91c1c"));
        require(window.statusLabel->text().contains("Cannot load font"),
                "Failed load did not show an error status");
        window.loadFont(fontDirectory + "/NotoColorEmoji.ttf");
        settle();
        checkStatus(window.statusLabel, QColor("#166534"));
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc != 2 && argc != 3) return 2;
    try {
        if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--load-font") {
            MainWindowTest::loadFont(QString::fromLocal8Bit(argv[2]));
            return 0;
        }
        MainWindowTest::run(QString::fromLocal8Bit(argv[1]));
        QPalette darkPalette = app.palette();
        darkPalette.setColor(QPalette::Window, QColor("#202020"));
        darkPalette.setColor(QPalette::Base, QColor("#151515"));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        app.setPalette(darkPalette);
        MainWindowTest::run(QString::fromLocal8Bit(argv[1]));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
