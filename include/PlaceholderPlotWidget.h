#pragma once

#include <QFrame>

class QLabel;

class PlaceholderPlotWidget final : public QFrame
{
    Q_OBJECT

public:
    explicit PlaceholderPlotWidget(const QString& title, QWidget* parent = nullptr);

    void SetMessage(const QString& message);

private:
    QLabel* titleLabel_ = nullptr;
    QLabel* messageLabel_ = nullptr;
};

