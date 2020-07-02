#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mesh.h"
#include "openglwidget.h"
//#include "pvl/PlyReader.hpp"
#include "las.h"
#include <QFileDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QProgressDialog>
#include <QShortcut>
#include <fstream>
#include <iostream>

static bool checkMod = true;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow) {
    ui_->setupUi(this);

    viewport_ = this->findChild<OpenGLWidget*>("Viewport");
    viewport_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QShortcut* showAll = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_A), this);
    QObject::connect(showAll, &QShortcut::activated, this, [this] {
        std::cout << "Showing all" << std::endl;
        QListWidget* list = this->findChild<QListWidget*>("MeshList");
        checkMod = false;
        for (int i = 0; i < list->count(); ++i) {
            list->item(i)->setCheckState(Qt::Checked);
        }
        checkMod = true;
    });
    for (int key = 0; key < 10; key++) {
        Qt::Key k = Qt::Key((key == 9) ? Qt::Key_0 : (Qt::Key_1 + key));
        QShortcut* showOnly = new QShortcut(QKeySequence(Qt::CTRL + k), this);
        QObject::connect(showOnly, &QShortcut::activated, this, [this, key] {
            std::cout << "Showing only " << key << std::endl;
            checkMod = false;
            QListWidget* list = this->findChild<QListWidget*>("MeshList");
            if (list->count() <= key) {
                return;
            }
            for (int i = 0; i < list->count(); ++i) {
                list->item(i)->setCheckState(i == key ? Qt::Checked : Qt::Unchecked);
            }
            checkMod = true;
        });

        QShortcut* showToggle = new QShortcut(QKeySequence(Qt::SHIFT + k), this);
        QObject::connect(showToggle, &QShortcut::activated, this, [this, key] {
            std::cout << "Showing toggle " << key << std::endl;
            checkMod = false;
            QListWidget* list = this->findChild<QListWidget*>("MeshList");
            if (list->count() <= key) {
                return;
            }
            Qt::CheckState state = list->item(key)->checkState();
            list->item(key)->setCheckState(state == Qt::Unchecked ? Qt::Checked : Qt::Unchecked);
            checkMod = true;
        });
    }

    QListWidget* list = this->findChild<QListWidget*>("MeshList");
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(list, &QListWidget::customContextMenuRequested, this, [this, list](const QPoint& pos) {
        QMenu submenu;
        QAction* actReload = submenu.addAction("Reload");
        QAction* actClose = submenu.addAction("Close");

        QPoint item = list->mapToGlobal(pos);
        QAction* clicked = submenu.exec(item);
        if (clicked == actClose) {
            viewport_->deleteMesh(list->currentItem());
            list->takeItem(list->currentIndex().row());
        } else if (clicked == actReload) {
            QString file = list->currentItem()->data(Qt::UserRole).toString();
            viewport_->deleteMesh(list->currentItem());
            list->takeItem(list->currentIndex().row());
            open(file);
        }
    });

    QShortcut* reloadAll = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_R), this);
    QObject::connect(reloadAll, &QShortcut::activated, this, [this] {
        std::cout << "Reloading all" << std::endl;
        QListWidget* list = this->findChild<QListWidget*>("MeshList");
        checkMod = false;
        for (QListWidgetItem* item : list->selectedItems()) {
            QString file = item->data(Qt::UserRole).toString();
            viewport_->deleteMesh(item);
            list->takeItem(list->row(item));
            open(file);
        }
        checkMod = true;
    });
}

MainWindow::~MainWindow() {
    delete ui_;
}

void MainWindow::open(const QString& file) {
    QCoreApplication::processEvents();
    try {
        QString ext = QFileInfo(file).suffix();
        if (ext != "ply" && ext != "las" && ext != "laz") {
            QMessageBox box(QMessageBox::Warning, "Error", "Unknown file format of file '" + file + "'");
            box.exec();
            return;
        }

        QProgressDialog dialog("Loading '" + file + "'", "Cancel", 0, 100);
        dialog.setWindowModality(Qt::WindowModal);
        auto callback = [&dialog](float prog) {
            dialog.setValue(prog);
            QCoreApplication::processEvents();
            return dialog.wasCanceled();
        };

        // Pvl::Optional<Mesh> mesh;
        Mesh mesh;
        if (ext == "ply") {
            std::ifstream in;
            in.exceptions(std::ifstream::badbit | std::ifstream::failbit);
            in.open(file.toStdString());
            // Pvl::Optional<Mesh> loaded
            mesh = loadPly(in, callback);
            /*if (!loaded) {
                return;
            }
            mesh = std::move(loaded.value());*/
        } else if (ext == "las" || ext == "laz") {
            mesh = loadLas(file.toStdString(), callback);
        }
        dialog.close();
        if (mesh.vertices.empty()) {
            return;
        }

        QListWidget* list = this->findChild<QListWidget*>("MeshList");
        QFileInfo info(file);
        QString identifier = info.absoluteDir().dirName() + "/" + info.baseName();
        QListWidgetItem* item = new QListWidgetItem(identifier, list);
        list->addItem(item);

        viewport_->view(item, std::move(mesh));
        item->setData(Qt::UserRole, file);
        item->setFlags(
            Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);

        /// \todo avoid firing signal

        item->setCheckState(Qt::CheckState::Checked);


    } catch (const std::exception& e) {
        QMessageBox box(QMessageBox::Warning, "Error", "Cannot open file '" + file + "'\n" + e.what());
        box.exec();
    }
}

void MainWindow::on_actionOpenFile_triggered() {
    QStringList names = QFileDialog::getOpenFileNames(
        this, tr("Open mesh"), ".", tr("all files (*);;.ply object (*.ply);;LAS point cloud (*.las *.laz)"));
    if (!names.empty()) {
        for (QString name : names) {
            open(name);
        }
    }
}
void MainWindow::on_MeshList_itemChanged(QListWidgetItem* item) {
    static int reentrant = 0;
    if (reentrant > 0) {
        return;
    }
    reentrant++;
    if (checkMod && QApplication::queryKeyboardModifiers() & Qt::CTRL) {
        QListWidget* list = item->listWidget();
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* it = list->item(i);
            it->setCheckState(it == item ? Qt::Checked : Qt::Unchecked);
            viewport_->toggle(it, it == item);
        }
    } else {
        bool on = item->checkState() == Qt::Checked;
        viewport_->toggle(item, on);
    }
    reentrant--;
}

void MainWindow::on_actionShowWireframe_changed() {
    QAction* act = this->findChild<QAction*>("actionShowWireframe");
    viewport_->wireframe(act->isChecked());
}

void MainWindow::on_actionShowDots_changed() {
    QAction* act = this->findChild<QAction*>("actionShowDots");
    viewport_->dots(act->isChecked());
}

void MainWindow::on_actionScreenshot_triggered() {
    QString file = QFileDialog::getSaveFileName(
        this, tr("Save screenshot"), ".", tr("PNG image (*.png);;JPEG image (*.jpg);;Targa image (*.tga)"));
    if (!file.isEmpty()) {
        if (QFileInfo(file).suffix().isEmpty()) {
            file += ".png";
        }
        viewport_->screenshot(file);
    }
}

void MainWindow::on_actionLaplacian_smoothing_triggered() {
    viewport_->laplacianSmooth();
}

void MainWindow::on_actionRepair_triggered() {
    viewport_->repair();
}

void MainWindow::on_actionSimplify_triggered() {
    viewport_->simplify();
}

void MainWindow::on_actionQuit_triggered() {
    close();
}

void MainWindow::on_actionSave_triggered() {
    QListWidget* list = this->findChild<QListWidget*>("MeshList");
    QListWidgetItem* item = list->currentItem();
    if (!item) {
        QMessageBox box(QMessageBox::Warning, "No selection", "Select a mesh to save");
        box.exec();
        return;
    }
    QString file = QFileDialog::getSaveFileName(this, tr("Save mesh"), ".", tr(".ply object (*.ply)"));
    if (!file.isEmpty()) {
        viewport_->saveMesh(file, item);
    }
}
