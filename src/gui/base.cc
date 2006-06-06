/** @file
 Base - class that host scripts and contain static script functions
 This class is also responsible for garbage collection of scripting objects
 @author Martin Petricek
*/

#include "base.h"
#include "aboutwindow.h"
#include "commandwindow.h"
#include "dialog.h"
#include "helpwindow.h"
#include "multitreewindow.h"
#include "menu.h"
#include "optionwindow.h"
#include "pagespace.h"
#include "pdfeditwindow.h"
#include "propertyeditor.h"
#include "qsannotation.h"
#include "qsarray.h"
#include "qscobject.h"
#include "qsdict.h"
#include "qsimporter.h"
#include "qsiproperty.h"
#include "qsipropertyarray.h"
#include "qsmenu.h"
#include "qspage.h"
#include "qspdf.h"
#include "qspdfoperator.h"
#include "qspdfoperatorstack.h"
#include "qstreeitem.h"
#include "settings.h"
#include "statusbar.h"
#include "treeitemabstract.h"
#include "util.h"
#include "version.h"
#include <string.h>
#include <delinearizator.h> 
#include <factories.h> 
#include <qdir.h>
#include <qfile.h>
#include <qmessagebox.h>
#include <qsinterpreter.h>
#include <qsplitter.h>
#include <qsutilfactory.h> 
#include <qsinputdialogfactory.h>
#include <utils/debug.h>

namespace gui {

using namespace util;

/**
 Create new Base class 
 @param parent Parent editor window containing this class
*/
Base::Base(PdfEditWindow *parent) {
 treeReloadFlag=false;
 w=parent;
 //Create new interpreter and project
 qp=new QSProject(this,"qs_project");
 qs=qp->interpreter();

 //Add ability to open files, directories and run processes
 qs->addObjectFactory(new QSUtilFactory());
 //Add ability to create dialogs
 qs->addObjectFactory(new QSInputDialogFactory());

 //Add settings object
 assert(globalSettings);
 qp->addObject(globalSettings);
 //Create and add importer to QSProject and related QSInterpreter
 import=new QSImporter(qp,this,this);
 import->addQSObj(w->pagespc,"PageSpace");
 import->addQSObj(w->cmdLine,"CommandWindow");
 qpdf=NULL;
}

/**
 Return interpreter instance used to launch scripts in this context
 @return current QSInterpreter
*/
QSInterpreter* Base::interpreter() {
 return qs;
}

/**
 Script error invoked when sxcript tried to access a NULL pointer
 @param className Name of class, in which this error occured
 @param methodName Name of method, in which this error occured
 */
void Base::errorNullPointer(const QString &className,const QString &methodName) {
 qs->throwError(QObject::tr("Null pointer access in ")+className+"."+methodName);
 //TODO: check
}

/** Import currently edited document (QSPDF wrapper) into scripting */
void Base::importDocument() {
 qpdf=import->createQSObject(w->document);
 import->addQSObj(qpdf,"document");
}

/** destroy document - destrouy it also in scripting */
void Base::destroyDocument() {
 if (qpdf) delete qpdf;
 qpdf=NULL;
}

/** if any script is running, stop it */
void Base::stopScript() {
 if (qs->isRunning()) {
  qs->stopExecution();
 }
}

/**
 Return QSA wrapper of current PDF document
 @return Current document (scripting wrapper)
 */
QSPdf* Base::getQSPdf() const {
 return qpdf;
}

/**
 Called after some action causes changes in the treeview that cannot be handled by obesrvers.
 This will cause tree to be reloaded after the script finishes. 
*/
void Base::treeNeedReload() {
 treeReloadFlag=true;
}

/** 
 Call a callback function (no arguments, no return value) in a script
 @param name Function name
*/
void Base::call(const QString &name) {
 guiPrintDbg(debug::DBG_INFO,"Performing callback: " << name);
 //Chekc if this call handler is called from a script
 bool running=qs->isRunning();
 if (!running) {
  //Do not tamper with the variables while the script is running
  addDocumentObjects();
 }
 try {
  //Call the function. Do not care about result
  qs->evaluate(name+"();",this,"<GUI>");
  if (globalSettings->readBool("console/show_handler_errors")) { //Show return value on console;
   QString error=qs->errorMessage();
   if (error!=QString::null) { /// some error occured
    w->cmdLine->addError(tr("Error in callback handler: ")+name);
    w->cmdLine->addError(error);
   }
  }
 } catch (...) {
  //Do not care about exception in callbacks either ... 
  if (globalSettings->readBool("console/show_handler_errors")) { //Show return value on console;
   w->cmdLine->addError(tr("Exception in callback handler: ")+name);
  }
 }
 if (!running) {
  //Do not tamper with the variables while the script is running
  //It may be undesirable to remove the objects while in script
  removeDocumentObjects();
  scriptCleanup();
 }
}

/** Cleanup run after the script is finished */
void Base::scriptCleanup() {
 if (treeReloadFlag) {
  //Reload the tree
  w->tree->reload();
  treeReloadFlag=false;
 }
}

/**
 For given name return widget represented by that name
 @param widgetName Widget name
 @return Pointer to widget, or NULL if no widget matches the name
*/
QWidget* Base::getWidgetByName(const QString &widgetName) {
 QString widget=widgetName.lower();
 if (widget=="commandline") return w->cmdLine;
 if (widget=="statusbar") return w->status;
 if (widget=="propertyeditor") return w->prop;
 if (widget=="rightside") return w->splProp;
 if (widget=="tree") return w->tree;
 //Widget not found ...
 return NULL;
}

/**
 Run all initscripts.
 Gets name of initscripts from settings
*/
void Base::runInitScript() {
 //Run list of initscripts from settings
 int scriptsRun=0;
 QStringList initScripts=globalSettings->readPath("init","script/");
 for (unsigned int i=0;i<initScripts.count();i++) {
  QString initScriptFilename=initScripts[i];
  guiPrintDbg(debug::DBG_INFO,"Considering init script: " << initScriptFilename);
  //Check if the script exists. If not, it is silently skipped
  if (exists(initScriptFilename)) {   
   guiPrintDbg(debug::DBG_INFO,"Running init script: " << initScriptFilename);
   //Any document-related classes are NOT available to the initscript, as no document is currently loaded
   runFile(initScriptFilename);
   scriptsRun++;
  }
 }
 guiPrintDbg(debug::DBG_DBG,"Initscripts executed");
 if (!scriptsRun) {
  //No init scripts found - print a warning
  warn(tr("No init script found - check your configuration")+"!\n"+tr("Looked for","scripts")+":\n"+initScripts.join("\n"));
 }
 // Run initscripts from paths listed in settings,
 // initscript with same name is executed only once,
 // initscripts in later paths take priority
 QStringList initScriptPaths=globalSettings->readPath("init_path","script/");
 QMap<QString,QString> initScriptAbsPaths;
 for (unsigned int ip=0;ip<initScriptPaths.count();ip++) {
  QString initPath=initScriptPaths[ip];
  if (!exists(initPath)) {
   guiPrintDbg(debug::DBG_WARN,"Init path does not exist: " << initPath);
   continue;
  }
  QDir dir(initPath);
  if (dir.isReadable()) {
   initScripts=dir.entryList("*.qs",QDir::Files | QDir::Readable,QDir::IgnoreCase | QDir::Name);
   for (unsigned int i=0;i<initScripts.count();i++) {
    QString initScriptFilename=initPath+"/"+initScripts[i];
    initScriptAbsPaths.insert(initScripts[i],initScriptFilename);
    guiPrintDbg(debug::DBG_INFO,"Adding init script: " << initScriptFilename);
   }
  }
  //Path is ok, check for scripts there
 }
 QMap<QString,QString>::Iterator it;
 for (it=initScriptAbsPaths.begin();it!=initScriptAbsPaths.end();++it) {
  QString initScriptFilename=it.data();
  guiPrintDbg(debug::DBG_INFO,"Running init script: " << initScriptFilename);
  //Any document-related classes are NOT available to the initscript, as no document is currently loaded
  runFile(initScriptFilename);    
 }
 guiPrintDbg(debug::DBG_DBG,"Initscripts from dirs executed");
}

/** Create objects that should be available to scripting from current CPdf and related objects*/
void Base::addDocumentObjects() {
 qs->setErrorMode(QSInterpreter::Nothing);
 //Import page and item (Currently selected page and currently selected object)
 QSCObject *trit=import->createQSObject(w->selectedTreeItem);
 QSCObject *item=NULL;
 if (w->selectedProperty.get()) {
  //IProperty is selected and will be imported
  item=import->createQSObject(w->selectedProperty);
 }
 if (w->selectedOperator.get()) {
  assert(!item);//Which would mean both iproperty and pdfoperator is selected-> bug
  //PdfOperator is selected and will be imported
  item=import->createQSObject(w->selectedOperator);
 }
 import->addQSObj(trit,"treeitem");
 import->addQSObj(item,"item");
}

/**
 Runs script from given file in current interpreter
 @param scriptName name of file with QT Script to run
 */
void Base::runFile(QString scriptName) {
 QString code=loadFromFile(scriptName);
 qs->evaluate(code,this,scriptName);
}

/**
 Add QSCObject to list of object to delete when file in editor window is closed
 @param o Object to add to cleanup-list
*/
void Base::addGC(QSCObject *o) {
 assert(o);
 baseObjects.replace(o,o);
}

/**
 Remove QSCObject from list of object to delete when file in editor window is closed
 @param o Object to remove from cleanup-list
*/
void Base::removeGC(QSCObject *o) {
 assert(o);
 baseObjects.remove(o);
}

/**
 Add tree item wrapper to list of managed wrappers<br>
 Called from treeitem wrapper contructor
 @param theWrap Tree item wrapper
*/
void Base::addTreeItemToList(QSTreeItem* theWrap) {
 TreeItemAbstract* theItem=theWrap->get();
 //Just insert to the list
 QPtrDict<void>* pDict=treeWrap[theItem];
 if (!pDict) {
  //We must dictionary to dictionary for this tree item
  pDict=new QPtrDict<void>(7);
  //Smaller dict, typically there will be few wrappers to same item
  treeWrap.insert(theItem,pDict); 
 }
 pDict->insert(theWrap,w);//Insert wrapper in dict
 guiPrintDbg(debug::DBG_DBG,"Added wrapper cleanly" << (intptr_t)theWrap); 
}

/**
 Remove tree item wrapper from list of managed wrappers<br>
 Called from treeitem wrapper contructor
 @param theWrap Tree item wrapper
*/
void Base::removeTreeItemFromList(QSTreeItem* theWrap) {
 TreeItemAbstract* theItem=theWrap->get();
 QPtrDict<void>* pDict=treeWrap[theItem];
 assert (pDict);//Not in list? WTF?
 assert (pDict->find(theWrap));//Not in list? WTF?
 pDict->remove(theWrap);
 guiPrintDbg(debug::DBG_DBG,"Removed wrapper cleanly" << (intptr_t)theWrap); 
}

/**
 Callback from main window when the treeitem got just deleted
 This will look for script wrappers containing the mentioned item and invalidate them, so they will not cause a crash
 @param theItem deleted tree item
*/
void Base::treeItemDeleted(TreeItemAbstract* theItem) {
 QPtrDict<void>* pDict=treeWrap[theItem];
 if (!pDict) {
  //No wrapper exists. Done.
  guiPrintDbg(debug::DBG_DBG,"Item deleted that is not in wrapper"); 
  return;
 }
 //Ok, now disable all wrappers pointing to this item
 guiPrintDbg(debug::DBG_DBG,"Will disable wrapped items"); 
 QPtrDictIterator<void> it(*pDict);
 for(;it.current();++it) {
  QSTreeItem* theWrap=reinterpret_cast<QSTreeItem*>(it.currentKey());
  guiPrintDbg(debug::DBG_DBG,"Disabling one wrapped item " << (intptr_t)theWrap); 
  assert(theWrap);
  guiPrintDbg(debug::DBG_DBG,"Check:" << theWrap->type()); 
  //Disable the wrapper, so calling it will not result in crash, but some error/exception instead
  theWrap->disable();
  guiPrintDbg(debug::DBG_DBG,"Disabled one wrapped item"); 
 }
 //Remove reference to subdictionary from dictionary
 treeWrap.remove(theItem);
 //Delete subdictionary (all wrappers in it were disabled)
 delete pDict;
}

/** Delete all objects in cleanup list */
void Base::cleanup() {
 guiPrintDbg(debug::DBG_INFO,"Garbage collection: " << baseObjects.count() << " objects");
 //Set autodelete and clear the list
 baseObjects.setAutoDelete(true);
 baseObjects.clear();
 baseObjects.setAutoDelete(false);
 //Delete list of tree wrappers, since all 
 guiPrintDbg(debug::DBG_INFO,"Garbage collection: " << treeWrap.count() << " items in tree wrap");
 treeWrap.clear();
 guiPrintDbg(debug::DBG_DBG,"Garbage collection done");
}

/**
 "Remove" defined variable from scripting context
 @param varName name of variable;
*/
void Base::deleteVariable(const QString &varName) {
 qs->setErrorMode(QSInterpreter::Nothing);
 qs->evaluate(varName+"=undefined;",this,"<delete_item>");
}

/** Removes objects added with addDocumentObject */
void Base::removeDocumentObjects() {
 //delete page and item variables from script -> they may change while script is not executing
 deleteVariable("item");
 deleteVariable("treeitem");
}

/**
 Runs given script code
 @param script QT Script code to run
*/
void Base::runScript(QString script) {
 qs->setErrorMode(QSInterpreter::Nothing);
 w->cmdLine->addCommand(script);
 /*
  Commit currently edited property in property editor
  This must be done this way, because by clicking on some button in toolbar to perform some action,
  for example reloading a page, does not cause the property to lose focus and update automatically
 */
 w->prop->commitProperty();
 //Before running script, add document-related objects to scripting engine and remove tham afterwards
 addDocumentObjects();
 QSArgument ret;
 try {
  guiPrintDbg(debug::DBG_DBG,"SCRIPT START"); 
  ret=qs->evaluate(script,this,"<GUI>");
  guiPrintDbg(debug::DBG_DBG,"SCRIPT STOP"); 
 } catch (...) {
  guiPrintDbg(debug::DBG_DBG,"CATCH"); 
  print(tr("Unknown exception in script occured"));
  guiPrintDbg(debug::DBG_DBG,"CATCH2");   
 }
 guiPrintDbg(debug::DBG_DBG,"CATCH AFTER"); 

 if (globalSettings->readBool("console/showretvalue")) { //Show return value on console;
  switch (ret.type()) {
   case QSArgument::QObjectPtr: { //QObject -> print type
    QObject *ob=ret.qobject();
    print(QString("(Object:")+ob->className()+")");
    break;
   }
   case QSArgument::VoidPointer: { //void * 
    print("(Pointer)");
    break;
   }
   case QSArgument::Variant: { //Variant - simple type.
    QVariant v=ret.variant();
    QString retVar=v.toString();
    if (!retVar.isNull()) print(retVar);
    break;
   }
   default: { 
    //Invalid - print nothing (void, etc ...)
   }
  }
 }
 QString error=qs->errorMessage();
 if (error!=QString::null) { /// some error occured
  w->cmdLine->addError(error);
 }
 removeDocumentObjects();
 scriptCleanup();
}

// === Non-scripting slots ===

#ifdef DRAGDROP
/**
 Invoked when dragging one item to another within the same tree window
 @param source Source (dragged) item
 @param target Target item
*/
void Base::_dragDrop(TreeItemAbstract *source,TreeItemAbstract *target) {
 qs->setErrorMode(QSInterpreter::Nothing);
 //Import items
 QSCObject *src=import->createQSObject(source);
 QSCObject *tgt=import->createQSObject(target);
 import->addQSObj(src,"source");
 import->addQSObj(tgt,"target");
 call("onDragDrop");
 qs->setErrorMode(QSInterpreter::Nothing);
 //delete page and item variables from script -> they may change while script is not executing
 deleteVariable("source");
 deleteVariable("target");
}

/**
 Invoked when dragging one item to another in different tree window (possibly in different document)
 @param source Source (dragged) item
 @param target Target item
*/
void Base::_dragDropOther(TreeItemAbstract *source,TreeItemAbstract *target) {
 /*
  More complicated, if the script keeps something from second tree
  and the second document is closed, the editor may crash.
  We need to make local copy of source's content
 */
 QSCObject *src=source->getQSObject(this);//Rebased object. May be NULL if rebase not possible
 QSCObject *tgt=import->createQSObject(target);
 import->addQSObj(src,"source");
 import->addQSObj(tgt,"target");
 call("onDragDropOther");
 deleteVariable("source");
 deleteVariable("target");
}

#endif

// === Scripting functions ===

/** Shows "About" window */
void Base::about() {
 AboutWindow *aboutWin= new AboutWindow(w);
 aboutWin->show();
}

/**
 Return active revision in current PDF document
 @return number of currently active revision
 */
int Base::activeRevision() {
 if (!w->document) return -1;
 return w->document->getActualRevision();
}

/** 
 Show dialog for adding objects into given container.<br>
 Container must be Dict or Array, otherwise the dialog is not created.
 If Container is NULL, currently selected object ihn property editor will be attempted to use as container..<br>
 After creating, dialog is shown and usable and this function immediately returns.<br>
 @param container Dict or Array into which objects will be added
*/
void Base::addObjectDialog(QSIProperty *container/*=NULL*/) {
 if (container) {
  w->addObjectDialogI(container->get());
 } else {
  w->addObjectDialogI(w->selectedProperty);
 }
}

/** \copydoc addObjectDialog(QSIProperty *) */
void Base::addObjectDialog(QObject *container) {
 //This method is fix for QSA Bug: QSA sometimes degrade QObject descendants to bare QObjects
 QSIProperty *_container=dynamic_cast<QSIProperty*>(container);
 if (_container) {
  w->addObjectDialogI(_container->get());
 } else {
  guiPrintDbg(debug::DBG_ERR,"type Error: " << container->className());
  w->addObjectDialogI(w->selectedProperty);
 }
}

/** \copydoc Menu::checkByName */
void Base::checkItem(const QString &name,bool check) {
 w->menuSystem->checkByName(name,check);
}

/**
 Create new operator of type UnknownPdfOperator
 @param parameters Array with operator parameters
 @param text Operator text
 @return new PDF operator
*/
QSPdfOperator* Base::createOperator(QSIPropertyArray* parameters,const QString &text) {
 std::string opTxt=text;
 PdfOperator::Operands param;
 parameters->copyTo(param);
 boost::shared_ptr<UnknownPdfOperator> op(new UnknownPdfOperator(param,opTxt));
 return new QSPdfOperator(op,this); 
}

/**
 Create new operator of type UnknownCompositePdfOperator
 @param beginText Start operator name text representation. 
 @param endText End operator name text representation.
 @return new PDF operator
*/
QSPdfOperator* Base::createCompositeOperator(const QString &beginText,const QString &endText) {
 boost::shared_ptr<UnknownCompositePdfOperator> op(new UnknownCompositePdfOperator(beginText,endText));
 return new QSPdfOperator(op,this); 
}

/**
 Create new empty (NULL) PdfOperator
 @return new empty PDF operator
*/
QSPdfOperator* Base::createEmptyOperator() {
 return new QSPdfOperator(this); 
}

/**
 Create and initialize new annotation of given type
 @param rect Annotation redctangle
 @param type Type of annotation
 @return created Annotation
*/
QSAnnotation* Base::createAnnotation(QVariant rect,const QString &type) {
 double tm[4]={0};
 QValueList<QVariant> list=rect.toList();
 QValueList<QVariant>::Iterator it = list.begin();
 int i=0;
 while(it!=list.end()) {
  if (i>=4) break;//We filled all values
  tm[i]=(*it).toDouble();
  ++it;
  ++i;
 }
 Rectangle rc(tm[0],tm[1],tm[2],tm[3]);
 boost::shared_ptr<CPage> nullPage;
 boost::shared_ptr<CAnnotation> annot=CAnnotation::createAnnotation(rc,type);
 return new QSAnnotation(annot,nullPage,this);
}

/**
 Create new IProperty of type Array - an empty array
 @return created IProperty
*/
QSIProperty* Base::createArray() {
 return new QSArray(boost::shared_ptr<CArray>(CArrayFactory::getInstance()),this);
}

/**
 Create new IProperty of type Bool
 @param value Value assigned to this property
 @return created IProperty
*/
QSIProperty* Base::createBool(bool value) {
 return new QSIProperty(boost::shared_ptr<IProperty>(CBoolFactory::getInstance(value)),this);
}

/**
 Create new IProperty of type Dict - an empty dictionary
 @return created IProperty
*/
QSIProperty* Base::createDict() {
 return new QSDict(boost::shared_ptr<CDict>(CDictFactory::getInstance()),this);
}

/**
 Create new IProperty of type Int
 @param value Value assigned to this property
 @return created IProperty
*/
QSIProperty* Base::createInt(int value) {
 return new QSIProperty(boost::shared_ptr<IProperty>(CIntFactory::getInstance(value)),this);
}

/**
 Create new IProperty of type Real
 @param value Value assigned to this property
 @return created IProperty
*/
QSIProperty* Base::createReal(double value) {
 return new QSIProperty(boost::shared_ptr<IProperty>(CRealFactory::getInstance(value)),this);
}

/**
 Create new IProperty of type Ref
 Does not check for validity of reference
 \see QSPdf::referenceValid
 @param valueNum Number assigned to this reference
 @param valueGen Generation assigned to this reference
 @return created IProperty
*/
QSIProperty* Base::createRef(int valueNum,int valueGen) {
 IndiRef ref;
 ref.num=valueNum;
 ref.gen=valueGen;
 return new QSIProperty(boost::shared_ptr<IProperty>(CRefFactory::getInstance(ref)),this);
}

/**
 Create new IProperty of type String
 @param value Value assigned to this property
 @return created IProperty
*/
QSIProperty* Base::createString(const QString &value) {
 return new QSIProperty(boost::shared_ptr<IProperty>(CStringFactory::getInstance(value)),this);
}

/**
 Create new IProperty of type Name
 @param value Value assigned to this property
 @return created IProperty
*/
QSIProperty* Base::createName(const QString &value) {
 return new QSIProperty(boost::shared_ptr<IProperty>(CNameFactory::getInstance(value)),this);
}

/**
 QSA-Bugfix version
 \copydoc createOperator(QSIPropertyArray*,const QString &)
*/
QSPdfOperator* Base::createOperator(QObject* parameters,const QString &text) {
 QSIPropertyArray* par=dynamic_cast<QSIPropertyArray*>(parameters);
 if (!par) return NULL;
 return createOperator(par,text);
}

//TODO: create all IProperty items (Simple types + array/dict/ref)

/**
 Create new array of IProperty items.
 This array can be used for example as operator parameters
 @return new Iproperty array
*/
QSIPropertyArray* Base::createIPropertyArray() {
 return new QSIPropertyArray(this);
}

/**
 Create new empty PDF Operator stack
 @return new operator stack
*/
QSPdfOperatorStack* Base::createPdfOperatorStack() {
 return new QSPdfOperatorStack(this);
}

/** create new empty editor window and display it */
void Base::createNewWindow() {
 PdfEditWindow::create();
}

/** \copydoc PdfEditWindow::closeFile */
bool Base::closeFile(bool askSave,bool onlyAsk/*=false*/) {
 return w->closeFile(askSave,onlyAsk);
}

/** \copydoc PdfEditWindow::closeWindow */
void Base::closeWindow() {
 w->closeWindow();
}

/**
 Try to delinearize PDF, reading from input file and writing delinearized result to output file.
 Does not check for overwriting output.
 Return true if delinearization was successful, false in case of failure.<br>
 In case of failure the error mesage is available via error()
 \see error
 @param infile input file
 @param outFile output file
*/
bool Base::delinearize(const QString &inFile,const QString &outFile) {
 utils::Delinearizator* delin=NULL;
 try {
  delin=utils::Delinearizator::getInstance(inFile,NULL);
  int ret=delin->delinearize(outFile);
  if (ret) {
   const char *whatWasWrong=strerror(ret);
   w->lastErrorMessage=whatWasWrong;
  } 
  if (delin) delete delin;  
  return (ret==0);
 } catch (...) {
  //This is the case of failure ..
  if (delin) delete delin;  
  return false;
 }
}

/** \copydoc Menu::enableByName */
void Base::enableItem(const QString &name,bool enable) {
 w->menuSystem->enableByName(name,enable);
}

/**
 Return last error message from some operations (like load, save, etc ...)
 If last command was successfull, it is undefined what this function returns
 @return Last error message 
 */
QString Base::error() {
 return w->lastErrorMessage;
}

/**
 Check whether given file exists
 @param chkFileName Name of file to check
 @return true if file exists, false otherwise
*/
bool Base::exists(const QString &chkFileName) {
  return QFile::exists(chkFileName);
}

/** \copydoc PdfEditWindow::exitApp */
void Base::exitApp() {
 w->exitApp();
}

/** \copydoc PdfEditWindow::filename() */
QString Base::filename() {
 return w->filename();
}

/**
 Show "open file" dialog and return file selected, or NULL if dialog was cancelled
 @return name of selected file.
 */
QString Base::fileOpenDialog() {
 guiPrintDbg(debug::DBG_DBG,"fileOpenDialog");
 return openFileDialogPdf(w);
}

/**
 Show "save file" dialog and return file selected, or NULL if dialog was cancelled
 @param oldName Old name of the file (if known) - will be preselected
 @return name of selected file.
 */
QString Base::fileSaveDialog(const QString &oldName/*=QString::null*/) {
 guiPrintDbg(debug::DBG_DBG,"fileSaveDialog");
 QString ret=saveFileDialogPdf(w,oldName);
 return ret;
}

/** Print all functions that are in current script interpreter to console window*/
void Base::functions() {
 QStringList objs=qs->functions(this);
 for (QStringList::Iterator it=objs.begin();it!=objs.end();++it) {
  print(*it);
 }
}

/**
 Invokes program help. Optional parameter is topic - if invalid or not defined, help title page will be invoked
 @param topic Starting help topic
*/
void Base::help(const QString &topic/*=QString::null*/) {
 HelpWindow *hb=new HelpWindow(topic);
 hb->show();
}

/**
 Check if a widget is visible
 \see getWidgetByName
 @param widgetName name of widget
 @return True if widget is visible, false if not
*/
bool Base::isVisible(const QString &widgetName) {
 QWidget *w=getWidgetByName(widgetName);
 if (!w) return false;
 return w->isVisible();
}

/**
 Load some PDF file without replacing currently opened file in GUI
 script should also take care to close the file after he does not need to use it anymore
 @param name Name of file to load
 @param advancedMode Set to true to use Advanced mode whilwe opening the file
 @return Loaded document, or NULL if error occured while loading it.
*/
QSPdf* Base::loadPdf(const QString &name,bool advancedMode/*=false*/) {
 if (name.isNull()) return NULL;
 CPdf::OpenMode mode=advancedMode?(CPdf::Advanced):(CPdf::ReadWrite);
 try {
  CPdf *opened=CPdf::getInstance(name,mode);
  //Return pdf wrapper with 'destructive close' behavior
  return new QSPdf(opened,this,true);
 } catch (...) {
  return NULL;
 }
}

/**
 Brings up informational messagebox with given message
 @param msg Message to display
 */
void Base::message(const QString &msg) {
 QMessageBox::information(w,APP_NAME,msg,QObject::tr("&Ok"),QString::null,QString::null,QMessageBox::Ok | QMessageBox::Default);
}

/** \copydoc PdfEditWindow::modified */
bool Base::modified() {
 return w->modified();
}

/** Print all objects that are in current script interpreter to console window*/
void Base::objects() {
 QObjectList objs=qs->presentObjects();
 QObjectListIterator it(objs);
 QObject *obj;
 while ((obj=it.current())!=0) {
  ++it;
  print(obj->name());
 }
}

/** \copydoc PdfEditWindow::openFile */
bool Base::openFile(const QString &name) {
 return w->openFile(name);
}

/**
 Open file in a new editor window.
 @param name Name of file to open
*/
void Base::openFileNew(const QString &name) {
 PdfEditWindow::create(name);
}

/** Show options dialog. Does not wait for dialog to finish. */
void Base::options() {
 OptionWindow::optionsDialog(w->menuSystem);
}

/**
 Return number of currently shown page
 @return page number
*/
int Base::pageNumber() {
 return w->selectedPageNumber;
}

/**
 Return currently shown page
 @return page
*/
QSPage* Base::page() {
 if (w->selectedPage.get()) {
  //REturn selected page
  return new QSPage(w->selectedPage,this);
 } else {
  //No page selected currently
  return NULL;
 }
}

/**
 Invoke dialog to select color.
 Last selected color is remembered and offered as default next time.
 The 'initial default color' is red
 @return selected color, or invalid color if the dialog was cancelled
*/
QVariant Base::pickColor() {
 QColor ret=colorDialog(w);
 if (!ret.isValid()) {
  return QVariant();
  guiPrintDbg(debug::DBG_DBG,"Color is not valid");
 }
 return QVariant(ret);
}

/**
 Create and return new popup menu, build from menu list/item  identified by this name.
 If no item specified, menu is initially empty
 @param menuName Name of menu inconfiguration to use as template
 @return initialized QSMenu Object
*/
QSMenu* Base::popupMenu(const QString &menuName/*=QString::null*/) {
 return new QSMenu(w->menuSystem,this,menuName);
}

/**
 Print given string to console, followed by newline
 @param str String to add
 */
void Base::print(const QString &str) {
 consoleLog(str,globalSettings->readExpand("path/console_log"));
 w->cmdLine->addString(str);
}

/**
 Asks question with Yes/No answer. "Yes" is default.
 Return true if user selected "yes", false if user selected "no"
 @param msg Question to display
 @return True if yes, false if no
*/
bool Base::question(const QString &msg) {
 return questionDialog(w,msg);
}

/**
 Asks question with Yes/No/Cancel answer. "Yes" is default. <br>
 Return one of these three according to user selection: <br>
  Yes : return 1     <br>
  No : return 0      <br>
  Cancel : return -1
 @param msg Question to display
 @return Selection of user.
 */
int Base::question_ync(const QString &msg) {
 int answer=QMessageBox::question(w,APP_NAME,msg,QObject::tr("&Yes"),QObject::tr("&No"),QObject::tr("&Cancel"),0,2);
 // QMessageBox::Yes | QMessageBox::Default,QMessageBox::No, QMessageBox::Cancel | QMessageBox::Escape
 switch (answer) {
  case 0: return 1;	//QMessageBox::Yes
  case 1: return 0;	//QMessageBox::No
  case 2: return -1;	//QMessageBox::Cancel
  default: assert(0);
 }
}

/** \copydoc PdfEditWindow::restoreWindowState */
void Base::restoreWindowState() {
 w->restoreWindowState();
}

/**
 Return number of revisions in current PDF document
 @return count of revisions
 */
int Base::revisions() {
 if (!w->document) return -1;
 return w->document->getRevisionsCount();
}

/**
 Runs script from given file
 Not to be called directly, only from script (via slot), as this does not prepare correct script variables before execution
 File is looked for in the script path, unless absolute filename is given.
 If the file is not found in script path, it is looked for in current directory
 @param scriptName name of file with QT Script to run
*/
void Base::run(QString scriptName) {
 runFile(globalSettings->getFullPathName("script",scriptName));
}

/**
 Save currently edited document to disk
 @return true if saved succesfully, false if failed to save because of any reason
 */
bool Base::save() {
 return w->save();
}

/**
 Save currently edited document to disk, while creating a new revisiion
 @return true if saved succesfully, false if failed to save because of any reason
 */
bool Base::saveRevision() {
 return w->save(true);
}

/** \copydoc PdfEditWindow::saveCopy */
bool Base::saveCopy(const QString &name) {
 return w->saveCopy(name);
}

/** \copydoc PdfEditWindow::saveWindowState */
void Base::saveWindowState() {
 w->saveWindowState();
}

/**
 Set widget to be either visible or invisible
 \see getWidgetByName
 @param widgetName name of widget
 @param visible action to be performed (true = show, false = hide)
*/
void Base::setVisible(const QString &widgetName, bool visible) {
 QWidget *w=getWidgetByName(widgetName);
 if (!w) return;
 if (visible) w->show(); else w-> hide();
}



/**
 Set new debug verbosity level
 @param param New debug verbosity level
 \see util::setDebugLevel
*/
void Base::setDebugLevel(const QString &param) {
 util::setDebugLevel(param);
}

/**
 Change active revision in current PDF document
 @param revision number of revision to activate
 */
void Base::setRevision(int revision) {
 w->changeRevision(revision);
}

/**
 call QObject::tr to translate specific string
 @param text text to translate to current locale
 @param context Optional context identifier for localized text
 @return translated text
*/
QString Base::tr(const QString &text,const QString &context/*=QString::null*/) {
 if (context.isNull()) return QObject::tr(text);
 return QObject::tr(text,context);
}

/**
 Return root item of currently selected tree
 @return Current tree root item
*/
QSTreeItem* Base::treeRoot() {
 return dynamic_cast<QSTreeItem*>(import->createQSObject(w->tree->root()));
}

/**
 Return root item of main tree
 @return Main tree root item
*/
QSTreeItem* Base::treeRootMain() {
 return dynamic_cast<QSTreeItem*>(import->createQSObject(w->tree->rootMain()));
}

/** Print all variables that are in current script interpreter to console window*/
void Base::variables() {
 QStringList objs=qs->variables(this);
 for (QStringList::Iterator it=objs.begin();it!=objs.end();++it) {
  print(*it);
 }
}

/** Return version of editor
 @return Version of editor (major.minor.release) */
QString Base::version() {
 return VERSION;
}

/**
 Show this string as a warning in a messagebox and also print it to console, followed by newline
 @param str String to use as warning
 */
void Base::warn(const QString &str) {
 print(str);
 QMessageBox::warning(w,tr("Warning"),str);
}

/** destructor */
Base::~Base() {
 delete import;
 delete qp;
}


} // namespace gui
