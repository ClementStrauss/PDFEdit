#ifndef __DIALOG_H__
#define __DIALOG_H__

#include <qwidget.h>
#include <qstring.h>

namespace gui {

QString openFileDialog(QWidget* parent=NULL);
QString saveFileDialog(QWidget* parent=NULL,const QString &oldname=QString::null,bool askOverwrite=true);
QString readStringDialog(QWidget* parent,const QString &message, const QString &def="");

} // namespace gui

#endif
