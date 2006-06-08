// vim:tabstop=4:shiftwidth=4:noexpandtab:textwidth=80
/*
 * =====================================================================================
 *        Filename:  testcpage.cc
 *         Created:  04/02/2006 15:47:27 AM CEST
 *          Author:  jmisutka (), 
 * =====================================================================================
 */

#include "testmain.h"
#include "testcobject.h"
#include "testcpage.h"
#include "testcpdf.h"

#include "factories.h"

#include <PDFDoc.h>
#include "../cpage.h"


//=====================================================================================

boost::shared_ptr<CPage> 
getPage (const char* fileName, boost::shared_ptr<CPdf> pdf, size_t pageNum)
{
	boost::scoped_ptr<PDFDoc> doc (new PDFDoc (new GString(fileName), NULL, NULL));
	
	//
	// Our stuff here
	//
	Object obj;
	XRef* xref = doc->getXRef();
	assert (xref);
	Catalog cat (xref);

	IndiRef ref;
	ref.num = cat.getPageRef(pageNum)->num;
	ref.gen = cat.getPageRef(pageNum)->gen;
	xref->fetch (ref.num, ref.gen, &obj);
	
	boost::shared_ptr<CDict> dict (new CDict (*pdf, obj, ref));
	obj.free ();
	
	return boost::shared_ptr<CPage> (new CPage(dict));
}

//=====================================================================================
namespace {
//=====================================================================================

using namespace boost;

//=====================================================================================

bool
mediabox (__attribute__((unused)) ostream& __attribute__((unused)) oss, const char* fileName)
{
	boost::shared_ptr<CPdf> pdf (getTestCPdf (fileName), pdf_deleter());

	for (size_t i = 0; i < pdf->getPageCount(); ++i)
	{
		boost::shared_ptr<CPage> page = pdf->getPage (i+1);

		try {
		/* oss << */ page->getMediabox ();
		}catch (...)
		{
			std::cout << "NO MEDIA BOX FOUND.";
			return true;
		}

		Rectangle rc;
		rc.xleft = 42;
		rc.yleft = 62;
		rc.xright = 12;
		rc.yright = 2342;
		page->setMediabox (rc);
		
		/* oss << */ page->getMediabox ();

		if (42 == page->getMediabox ().xleft &&
				62 == page->getMediabox ().yleft)
			continue;
		else
			return false;

		_working (oss);
	}

	return true;
}


//=====================================================================================

bool
display (__attribute__((unused)) ostream& oss, const char* fileName)
{
	// Open pdf and get the first page	
	boost::shared_ptr<CPdf> pdf (getTestCPdf (fileName), pdf_deleter());

	for (size_t i = 0; i < pdf->getPageCount() && i < TEST_MAX_PAGE_COUNT; ++i)
	{
		boost::shared_ptr<CPage> page = pdf->getPage (i+1);

		//TextOutputDev textOut (NULL, gTrue, gFalse, gTrue);
		const char* FILE_OUT = "1.txt";
		TextOutputDev textOut (const_cast<char*>(FILE_OUT), gFalse, gFalse, gFalse);
		if (!textOut.isOk ())
			throw;
		
		//
		// Output to file
		//
		//oss << "Creating 1.txt which contains text from a pdf." << endl;
		page->displayPage (textOut);
		
		xpdf::openXpdfMess ();

		//oss << "Output from textoutputdevice." << endl;
		//oss << textOut.getText(0, 0, 1000, 1000)->getCString() << endl;
		delete textOut.getText(0, 0, 1000, 1000);
		xpdf::closeXpdfMess ();

		_working (oss);
	}
	
	return true;
}

//=====================================================================================

bool
_export (__attribute__((unused)) ostream& oss, const char* fileName)
{
	// Open pdf and get the first page	
	boost::shared_ptr<CPdf> pdf (getTestCPdf (fileName), pdf_deleter());

	for (size_t i = 0; i < pdf->getPageCount() && i < TEST_MAX_PAGE_COUNT; ++i)
	{
		boost::shared_ptr<CPage> page = pdf->getPage (i+1);

		string tmp;
		page->getText (tmp);
		//oss << "Text: " << tmp << endl;
		_working (oss);
	}
	
	return true;
}

//=====================================================================================

bool
findtext (__attribute__((unused)) ostream& oss, const char* fileName)
{
	// Open pdf and get the first page	
	boost::shared_ptr<CPdf> pdf (getTestCPdf (fileName), pdf_deleter());

	for (size_t i = 0; i < pdf->getPageCount() && i < TEST_MAX_PAGE_COUNT; ++i)
	{
		boost::shared_ptr<CPage> page = pdf->getPage (i+1);

		string tmp;
		page->getText (tmp);
		//oss << "Text: " << tmp << endl;


		typedef std::vector<Rectangle> Recs;
		Recs recs;
		string word;
		if (tmp.length() > 10)
		{
			// arbitrary word/string??
			word = tmp.substr (2,3);
			page->findText (word, recs); 
			if (recs.empty())
			{
					oss << "RECS ARE EMPTY !!!" << flush;
					//getchar ();
			}else
				oss << "Text: " << word << " at position: " << recs.front() << flush;
		}
	}
	
	return true;
}


//=====================================================================================

bool
getSetFonts (__attribute__((unused)) ostream& oss, const char* fileName)
{
	{
		// Open pdf and get the first page	
		boost::shared_ptr<CPdf> pdf (getTestCPdf (fileName), pdf_deleter());

		for (size_t i = 0; i < pdf->getPageCount() && i < TEST_MAX_PAGE_COUNT; ++i)
		{
			boost::shared_ptr<CPage> page = pdf->getPage (i+1);

			typedef vector<pair<string,string> > Fonts;
			Fonts fs;
			
			page->getFontIdsAndNames (fs);

			if (fs.empty())		
				oss << "FONTS ARE EMPTY !!!" << flush;
			else
			{
				oss << "Fonts on " << (i + 1) << "-th page: " << flush;
				for (Fonts::iterator it = fs.begin(); it != fs.end(); ++it)
					oss << "(" << (*it).first << ", " << (*it).second << ") " << flush;
			}
			
		}
	}

	{
		// Open pdf and get the first page	
		boost::shared_ptr<CPdf> pdf (getTestCPdf (fileName), pdf_deleter());

		for (size_t i = 0; i < pdf->getPageCount() && i < TEST_MAX_PAGE_COUNT; ++i)
		{
			boost::shared_ptr<CPage> page = pdf->getPage (i+1);

			typedef vector<pair<string,string> > Fonts;
			Fonts fs;
			
			const string fn ("Jozov-font");
			page->addSystemType1Font (fn);
			page->getFontIdsAndNames (fs);

			CPPUNIT_ASSERT (!fs.empty());

			bool found = false;
			for (Fonts::iterator it = fs.begin(); it != fs.end(); ++it)
			{
				if (fn == (*it).second)
				{
					found = true;
					break;
				}
			}

			CPPUNIT_ASSERT (found);
			
			page->getFontIdsAndNames (fs);
			CPPUNIT_ASSERT (!fs.empty());
			//oss << "After change: " << (i + 1) << "-th page: " << flush;
			for (Fonts::iterator it = fs.begin(); it != fs.end(); ++it)
			{
			//	oss << "(" << (*it).first << ", " << (*it).second << ") " << flush;
			}
			
			{
				page->addSystemType1Font (fn);
				page->addSystemType1Font (fn);
				page->addSystemType1Font (fn);

				page->getFontIdsAndNames (fs);
				CPPUNIT_ASSERT (!fs.empty());
				oss << "After change: " << (i + 1) << "-th page: " << flush;
				for (Fonts::iterator it = fs.begin(); it != fs.end(); ++it)
					oss << "(" << (*it).first << ", " << (*it).second << ") " << flush;
			}

		}

	}

	
	return true;
}


//=====================================================================================
bool setattr(__attribute__((unused))	ostream & oss, const char * fname)
{
	CPdf * pdf=getTestCPdf(fname);

	size_t pageCount=pdf->getPageCount();
	for(size_t pos=1; pos<=pageCount; pos++)
	{
		shared_ptr<CPage> page=pdf->getPage(pos);

		page->setMediabox (Rectangle (1,1,2,2));
		Rectangle rc = page->getMediabox ();
		CPPUNIT_ASSERT (rc.xleft == 1 && rc.yleft == 1);
		CPPUNIT_ASSERT (rc.xright == 2 && rc.yright == 2);
		page->setRotation (10);
		CPPUNIT_ASSERT (10 == page->getRotation());

	}
	pdf->close();
	return true;
}



//=====================================================================================
bool creation (__attribute__((unused)) ostream& oss)
{
	shared_ptr<CDict> dict (CDictFactory::getInstance());
	CArray array;
	// Uuuuuhhhhhhhhhhh
	shared_ptr<CPage> page (new CPage(dict));
	//CPPUNIT_ASSERT (false == page->parseContentStream ());

	dict->addProperty ("Contents", array);
	//CPPUNIT_ASSERT (true == page->parseContentStream ());

	return true;
}

bool annotsTests(ostream & oss, const char * fname)
{
	CPdf * pdf=getTestCPdf(fname);

	size_t pageCount=pdf->getPageCount();
	for(size_t pos=1; pos<=pageCount; pos++)
	{
		shared_ptr<CPage> page=pdf->getPage(pos);
		CPage::AnnotStorage annots;
		page->getAllAnnotations(annots);
		if(annots.empty())
			continue;

		oss << "Page #"<<pos<<" has "<<annots.size()<<" annotations"<<endl;

	}
	pdf->close();
	return true;
}


//=========================================================================
// class TestCPage
//=========================================================================

class TestCPage : public CppUnit::TestFixture 
{
	CPPUNIT_TEST_SUITE(TestCPage);
		CPPUNIT_TEST(TestFonts);
		CPPUNIT_TEST(Test);
		CPPUNIT_TEST(TestCreation);
		CPPUNIT_TEST(TestDisplay);
		CPPUNIT_TEST(TestExport);
		CPPUNIT_TEST(TestFind);
		CPPUNIT_TEST(TestSet);
		CPPUNIT_TEST(TestAnnotations);
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp() {OUTPUT << endl;}
	void tearDown() {}

public:
	//
	//
	//
	void Test ()
	{
		OUTPUT << "CPage methods..." << endl;

		for (FileList::const_iterator it = fileList.begin (); it != fileList.end(); ++it)
		{
			OUTPUT << "Testing filename: " << *it << endl;
		
			TEST(" mediabox");
			CPPUNIT_ASSERT (mediabox (OUTPUT, (*it).c_str()));
			OK_TEST;
		}
	}
	//
	//
	//
	void TestSet ()
	{
		OUTPUT << "CPage methods..." << endl;

		for (FileList::const_iterator it = fileList.begin (); it != fileList.end(); ++it)
		{
			OUTPUT << "Testing filename: " << *it << endl;
		
			TEST(" set");
			CPPUNIT_ASSERT (setattr (OUTPUT, (*it).c_str()));
			OK_TEST;
		}
	}

	//
	//
	//
	void TestDisplay ()
	{
		OUTPUT << "CPage display methods..." << endl;
		
		for (FileList::const_iterator it = fileList.begin (); it != fileList.end(); ++it)
		{
			OUTPUT << "Testing filename: " << *it << endl;
			
			TEST(" display");
			CPPUNIT_ASSERT (display (OUTPUT, (*it).c_str()));
			OK_TEST;
		}
	}
	//
	//
	//
	void TestCreation ()
	{
		OUTPUT << "CPage creation methods..." << endl;
		
		TEST(" creation");
		CPPUNIT_ASSERT (creation (OUTPUT));
		OK_TEST;
	}
	//
	//
	//
	void TestExport ()
	{
		OUTPUT << "CPage export..." << endl;
		
		for (FileList::const_iterator it = fileList.begin (); it != fileList.end(); ++it)
		{
			OUTPUT << "Testing filename: " << *it << endl;
			
			TEST(" export");
			CPPUNIT_ASSERT (_export (OUTPUT, (*it).c_str()));
			OK_TEST;
		}
	}
	//
	//
	//
	void TestFind ()
	{
		OUTPUT << "CPage find..." << endl;
		
		for (FileList::const_iterator it = fileList.begin (); it != fileList.end(); ++it)
		{
			OUTPUT << "Testing filename: " << *it << endl;
			
			TEST(" find text");
			CPPUNIT_ASSERT (findtext (OUTPUT, (*it).c_str()));
			OK_TEST;
		}
	}
	//
	//
	//
	void TestFonts ()
	{
		OUTPUT << "CPage fonts..." << endl;
		
		for (FileList::const_iterator it = fileList.begin (); it != fileList.end(); ++it)
		{
			OUTPUT << "Testing filename: " << *it << endl;
			
			TEST(" get font names");
			CPPUNIT_ASSERT (getSetFonts (OUTPUT, (*it).c_str()));
			OK_TEST;
		}
	}
	//
	//
	//
	void TestAnnotations()
	{
		OUTPUT << "CPage annotations..."<<endl;

		for (FileList::const_iterator it = fileList.begin (); it != fileList.end(); ++it)
		{
			OUTPUT << "Testing filename: " << *it << endl;
			
			TEST(" annotations tests");
			CPPUNIT_ASSERT (annotsTests(OUTPUT, (*it).c_str()));
			OK_TEST;
		}
	}

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestCPage);
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestCPage, "TEST_CPAGE");

//=====================================================================================
} // namespace
//=====================================================================================








