#ifndef IU_CORE_HTMLELEMENT_H
#define IU_CORE_HTMLELEMENT_H 
#include <Core/Utils/CoreTypes.h>
#include <Core/Squirrelnc.h>
namespace ScriptAPI {

class HtmlElementPrivate;

class HtmlElement {
public:
	HtmlElement();
	HtmlElement(HtmlElementPrivate* pr);

	const std::string getAttribute(const std::string& name);
	void setAttribute(const std::string& name, const std::string& value);
	void removeAttribute(const std::string& name);
	const std::string getId();
	void setId(const std::string& id);
	const std::string getInnerHTML();
	void setInnerHTML(const std::string& html);
	const std::string getInnerText();
	void setInnerText(const std::string& text);
	const std::string getOuterHTML();
	void setOuterHTML(const std::string& html);
	const std::string getOuterText();
	void setOuterText(const std::string& text);
	void setValue(const std::string& value);
	const std::string getValue();
	const std::string getTagName();
	HtmlElement getParentElement();
	void scrollIntoView();
	void click();
	void insertHTML(const std::string& name, bool atEnd = false );
	void insertText(const std::string& name, bool atEnd = false );
	HtmlElement querySelector(const std::string& query);
	SquirrelObject querySelectorAll(const std::string& query);
	SquirrelObject getFormElements();
	bool submitForm();
	bool isNull();
	SquirrelObject getChildren();
	friend class HtmlDocumentPrivate;
protected:
	std_tr::shared_ptr<HtmlElementPrivate> d_;
	bool checkNull(const char * func);
};

void RegisterHtmlElementClass();

}
#endif