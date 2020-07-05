#pragma once

#include "renderer.h"
#include <QFileDialog>
#include <QMainWindow>
#include <QPainter>
#include <QResizeEvent>
#include <iostream>

QT_BEGIN_NAMESPACE
namespace Ui {
class FrameBuffer;
}
class QProgressBar;
QT_END_NAMESPACE

struct TaskGroup;


class View : public QWidget {
public:
    View(QWidget* parent)
        : QWidget(parent) {}

    virtual void paintEvent(QPaintEvent*) override;

    void setTaskGroup(std::shared_ptr<TaskGroup> tg);

    void setImage(FrameBuffer&& image);

    void setExposure(int exposure);

    void save();

private:
    FrameBuffer fb_;
    std::shared_ptr<TaskGroup> tg_;
    float exposure_ = 1.f;
};


class FrameBufferWidget : public QMainWindow {
    Q_OBJECT

public:
    FrameBufferWidget(QWidget* parent = nullptr);
    ~FrameBufferWidget();

    void setImage(FrameBuffer&& image) {
        view_->setImage(std::move(image));
    }

    void setProgress(const float prog);

    void run(const std::function<void()>& func);

private slots:
    void on_actionSave_render_triggered();

    void on_horizontalSlider_valueChanged(int value);

private:
    Ui::FrameBuffer* ui_;
    View* view_;
    QProgressBar* progressBar_;
    float progressValue_ = 0.f;
    std::shared_ptr<TaskGroup> tg_;
};