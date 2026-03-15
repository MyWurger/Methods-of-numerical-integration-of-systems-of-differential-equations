#include "MainWindow.h"
#include "TEuler.h"
#include "TRungeKutta.h"

#include <QApplication>
#include <QFont>

#include <memory>
#include <vector>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Моделирование движения КА");
    app.setOrganizationName("ModelingMethodsLab1");

    QFont uiFont("Avenir Next", 11);
    uiFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(uiFont);

    std::vector<IntegratorOption> integratorOptions;
    integratorOptions.push_back(
        {"Метод Эйлера",
         [](double t0, double tk, double h)
         {
             return std::make_unique<TEuler>(t0, tk, h);
         }});
    integratorOptions.push_back(
        {"Метод Рунге-Кутты 4 порядка",
         [](double t0, double tk, double h)
         {
             return std::make_unique<TRungeKutta>(t0, tk, h);
         }});

    MainWindow window(std::move(integratorOptions));
    window.show();

    return app.exec();
}

