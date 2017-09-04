#include <QtGui/QApplication>
#include <QtCore/QSysInfo>
#include "MainUnt.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QStringList matchlist = QString::fromUtf16((ushort*)L"Tahoma;ËÎÌå;Î¢ÈíÑÅºÚ;£Í£Ó ¥´¥·¥Ã¥¯").split(";", QString::KeepEmptyParts, Qt::CaseInsensitive);
    QFont::insertSubstitutions("Tahoma", matchlist);
    QFont::insertSubstitutions("MS Shell Dlg2", matchlist);
    QFont f(QLatin1String("Tahoma"), 8);
    app.setFont(f);

    //if (QSysInfo::WindowsVersion < QSysInfo::WV_5_1) {
    //    QApplication::setStyle("CleanLooks");
    //} else {
    //    typedef int (__stdcall *FARPROC)();
    //    QLibrary uxthemelib("uxtheme");
    //    FARPROC pIsAppThemed = FARPROC(uxthemelib.resolve("IsAppThemed"));
    //    if (pIsAppThemed && pIsAppThemed() == FALSE) {
    //        QApplication::setStyle("CleanLooks");
    //    }
    //}
    QApplication::setStyle("CleanLooks");

    TMainFrm mainfrm;
    mainfrm.show();
    return app.exec();
}
