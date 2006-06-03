#ifndef __TREEITEMOPERATORCONTAINER_H__
#define __TREEITEMOPERATORCONTAINER_H__

#include <iproperty.h>
#include <qlistview.h>
#include "treeitemabstract.h"
#include "types.h"
class QString;

namespace gui {

using namespace pdfobjects;

class TreeData;

/**
 Class holding vector with pdf operators
 The vector is specified in constructor and comes from arbitrary source
 (usually selected objects from page)
*/
class TreeItemOperatorContainer : public TreeItemAbstract {
public:
 TreeItemOperatorContainer(TreeData *_data,QListView *parent,const OperatorVector &_op,const QString name=QString::null,QListViewItem *after=NULL,const QString &nameId=NULL);
 TreeItemOperatorContainer(TreeData *_data,QListViewItem *parent,const OperatorVector &_op,const QString name=QString::null,QListViewItem *after=NULL,const QString &nameId=NULL);
 virtual ~TreeItemOperatorContainer();
 //From TreeItemAbstract interface
 virtual ChildType getChildType(const QString &name);
 virtual TreeItemAbstract* createChild(const QString &name,ChildType typ,QListViewItem *after=NULL);
 virtual QStringList getChildNames();
 virtual bool haveChild();
 virtual QSCObject* getQSObject();
 virtual void remove();
 virtual void reloadSelf();
private:
 void init(const QString &name);
private:
 /** Vector with pdf operators */
 OperatorVector op;

};

} // namespace gui

#endif