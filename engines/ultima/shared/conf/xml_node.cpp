/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "ultima/shared/conf/xml_node.h"
#include "ultima/shared/conf/xml_tree.h"
#include "common/file.h"

namespace Ultima {
namespace Shared {

static Common::String encodeEntity(const Common::String &s);
static Common::String closeTag(const Common::String &s);

XMLNode::~XMLNode() {
	for (Common::Array<XMLNode *>::iterator i = _nodeList.begin();
			i != _nodeList.end(); ++i) {
		delete *i;
	}
}

const Common::String &XMLNode::reference(const Common::String &h, bool &exists) {
	if (h.find('/') == Common::String::npos) {
		// Must refer to me.
		if (_id == h) {
			exists = true;
			return _content;
		}
	} else {
		// Otherwise we want to split the Common::String at the first /
		// then locate the branch to walk, and pass the rest
		// down.

		Common::String k;
		k = h.substr(h.find('/') + 1);
		Common::String k2 = k.substr(0, k.find('/'));
		for (Common::Array<XMLNode *>::iterator it = _nodeList.begin();
		        it != _nodeList.end(); ++it) {
			if ((*it)->_id == k2)
				return (*it)->reference(k, exists);
		}
	}

	exists = false;
	return _emptyString;
}


const XMLNode *XMLNode::subtree(const Common::String &h) const {
	if (h.find('/') == Common::String::npos) {
		// Must refer to me.
		if (_id.equalsIgnoreCase(h))
			return this;
	} else {
		// Otherwise we want to split the Common::String at the first /
		// then locate the branch to walk, and pass the rest
		// down.

		Common::String k;
		k = h.substr(h.find('/') + 1);
		Common::String k2 = k.substr(0, k.find('/'));
		for (Common::Array<XMLNode *>::const_iterator it = _nodeList.begin();
		        it != _nodeList.end(); ++it) {
			if ((*it)->_id.equalsIgnoreCase(k2)) {
				return (*it)->subtree(k);
			}
		}
	}

	return nullptr;
}


Common::String XMLNode::dump(int depth) {
	Common::String s;
	for (int i = 0; i < depth; ++i)
		s += ' ';

	s += "<";
	s += _id;
	s += ">";
	if (_id[_id.size() - 1] != '/') {
		if (_nodeList.empty() == false)
			s += "\n";
		for (Common::Array<XMLNode *>::const_iterator it = _nodeList.begin();
		        it != _nodeList.end(); ++it) {
			s += (**it).dump(depth + 1);
		}

		if (_content.size()) {
			//s += Common::String(depth,' ');
			s += encodeEntity(_content);
		}
		if (_id[0] == '?') {
			return s;
		}
		//if(_content.size())
		//  s += "\n";

		if (!_noClose) {
			if (!_content.size()) {
				for (int i = 0; i < depth; ++i)
					s += ' ';
			}

			s += "</";
			s += closeTag(_id);
			s += ">\n";
		}
	}

	return s;
}

void XMLNode::xmlAssign(const Common::String &key, const Common::String &value) {
	if (key.find('/') == Common::String::npos) {
		// Must refer to me.
		if (_id == key)
			_content = value;
		else
			error("Walking the XML tree failed to create a final node.");
		return;
	}
	Common::String k;
	k = key.substr(key.find('/') + 1);
	Common::String k2 = k.substr(0, k.find('/'));
	for (Common::Array<XMLNode *>::iterator it = _nodeList.begin();
	        it != _nodeList.end(); ++it) {
		if ((*it)->_id == k2) {
			(**it).xmlAssign(k, value);
			return;
		}
	}

	// No match, so create a new node and do recursion
	XMLNode *t = new XMLNode(_tree);
	t->_parent = this;
	t->_id = k2;
	_nodeList.push_back(t);
	(*t).xmlAssign(k, value);
}


void XMLNode::listKeys(const Common::String &key, Common::Array<Common::String> &vs,
		bool longformat) const {
	Common::String s(key);
	s += "/";

	for (Common::Array<XMLNode *>::const_iterator it = _nodeList.begin();
	        it != _nodeList.end(); ++it) {
		if (!longformat)
			vs.push_back((*it)->_id);
		else
			vs.push_back(s + (*it)->_id);
	}
}

static Common::String encodeEntity(const Common::String &s) {
	Common::String  ret;

	for (Common::String::const_iterator it = s.begin(); it != s.end(); ++it) {
		switch (*it) {
		case '<':
			ret += "&lt;";
			break;
		case '>':
			ret += "&gt;";
			break;
		case '"':
			ret += "&quot;";
			break;
		case '\'':
			ret += "&apos;";
			break;
		case '&':
			ret += "&amp;";
			break;
		default:
			ret += *it;
		}
	}
	return ret;
}

static Common::String decode_entity(const Common::String &s, size_t &pos) {
//	size_t old_pos = pos;
	size_t entityNameLen = s.findFirstOf("; \t\r\n", pos) - pos - 1;

	/* Call me paranoid... but I don't think having an end-of-line or similar
	    inside a &...; expression is 'good', valid though it may be. */
	assert(s[pos + entityNameLen + 1] == ';');

	Common::String entity_name = s.substr(pos + 1, entityNameLen);

	pos += entityNameLen + 2;

	// Std::cout << "DECODE: " << entity_name << endl;

	if (entity_name == "amp")
		return Common::String("&");
	else if (entity_name == "apos")
		return Common::String("'");
	else if (entity_name == "quot")
		return Common::String("\"");
	else if (entity_name == "lt")
		return Common::String("<");
	else if (entity_name == "gt")
		return Common::String(">");
	else if (entity_name.hasPrefix("#")) {
		entity_name.deleteChar(0);

		if (entity_name.hasPrefix("x")) {
			uint tmp = 0;
			int read = sscanf(entity_name.c_str() + 1, "%xh", &tmp);
			if (read < 1)
				error("strToInt failed on string \"%s\"", entity_name.c_str());
			return Common::String((char)tmp);
		} else {
			uint tmp = atol(entity_name.c_str());
			return Common::String((char)tmp);
		}
	} else {
		error("Invalid xml encoded entity - %s", entity_name.c_str());
	}
}

static Common::String closeTag(const Common::String &s) {
	if (s.find(" ") == Common::String::npos)
		return s;

	return s.substr(0, s.find(" "));
}


static void trim(Common::String &s) {
	// Clean off leading whitespace
	while (s.size() && s[0] <= 32) {
		s = s.substr(1);
	}
	// Clean off trailing whitespace
	while (s.size() && s[s.size() - 1] <= 32) {
		s.erase(s.size() - 1);
	}
}

XMLNode *XMLNode::xmlParseDoc(XMLTree *tree, const Common::String &s) {
	Common::String sbuf(s);
	size_t nn = 0;
	bool parsedXmlElement = false, parsedDocType = false;
	XMLNode *node = nullptr, *child = nullptr;

	for (;;) {
		while (nn < s.size() && Common::isSpace(s[nn]))
			++nn;
		if (nn >= s.size())
			return node;

		if (s[nn] != '<') {
			warning("expected '<' while reading config file, found %c\n", s[nn]);
			return nullptr;
		}
		++nn;

		if (nn < s.size() && s[nn] == '?') {
			assert(!parsedXmlElement);
			parsedXmlElement = true;
			nn = s.findFirstOf('>', nn);
		} else if (nn < s.size() && s.substr(nn, 8).equalsIgnoreCase("!doctype")) {
			assert(!parsedDocType);
			parsedDocType = true;
			parseDocTypeElement(s, nn);
		} else {
			child = xmlParse(tree, sbuf, nn);
			if (child) {
				if (node)
					error("Invalid multiple xml nodes at same level");
				node = child;
			}

			continue;
		}

		// If this point was reached, we just skipped ?xml or doctype element
		++nn;
	}

	return node;
}

void XMLNode::parseDocTypeElement(const Common::String &s, size_t &nn) {
	nn = s.findFirstOf(">[", nn);
	if (nn == Common::String::npos)
		// No ending tag
		return;

	if (s[nn] == '[') {
		// Square bracketed area
		nn = s.findFirstOf(']', nn) + 1;
	}

	if (nn >= s.size() || s[nn] != '>')
		nn = Common::String::npos;
}

XMLNode *XMLNode::xmlParse(XMLTree *tree, const Common::String &s, size_t &pos) {
	bool intag = true;
	XMLNode *node = nullptr;
	Common::String nodeText;

	while (pos < s.size()) {
		if (nodeText.hasPrefix("!--") && (s[pos] != '>' || !nodeText.hasSuffix("--"))) {
			// It's a > within the comment, but not the terminator
			nodeText += s[pos];
			++pos;
			continue;
		}

		switch (s[pos]) {
		case '<': {
			// New tag?
			if (s[pos + 1] == '/') {
				// No. Close tag.
				while (s[pos] != '>')
					pos++;
				++pos;
				trim(node->_content);
				return node;
			}

			++pos;
			XMLNode *t = xmlParse(tree, s, pos);
			if (t) {
				t->_parent = node;
				node->_nodeList.push_back(t);
			}

			break;
		}
		case '>':
			// End of tag
			if (s[pos - 1] == '/') {
				if (s[pos - 2] == '<') {
					++pos;
					trim(node->_content);
					return node; // An empty tag
				} else {
					++pos;
					nodeText.deleteLastChar();	// Remove ending /
					node = new XMLNode(tree);
					node->parseNodeText(nodeText);

					if (node->_id.equalsIgnoreCase("xi:include")) {
						Common::String fname = node->_attributes["href"];
						delete node;
						node = xmlParseFile(tree, fname);
					}

					return node;
				}
			} else if (nodeText.hasPrefix("!--")) {
				// Comment element
				++pos;
				return nullptr;
			}

			assert(!node);
			node = new XMLNode(tree);
			node->parseNodeText(nodeText);

			++pos;
			intag = false;
			if (s[pos] < 32)
				++pos;
			break;
		case '&':
			if (intag)
				nodeText += decode_entity(s, pos);
			else
				node->_content += decode_entity(s, pos);
			break;
		default:
			if (intag)
				nodeText += s[pos++];
			else
				node->_content += s[pos++];
		}
	}

	trim(node->_content);
	return node;
}

void XMLNode::parseNodeText(const Common::String &nodeText) {
	size_t firstSpace = nodeText.findFirstOf(' ');
	if (firstSpace == Common::String::npos) {
		// The entire text is the id
		_id = nodeText;
		return;
	}

	// Set the Id and get out the remaining attributes section, if any
	_id = Common::String(nodeText.c_str(), firstSpace);
	Common::String attr(nodeText.c_str() + firstSpace);

	for (;;) {
		// Skip any spaces
		while (!attr.empty() && Common::isSpace(attr[0]))
			attr.deleteChar(0);
		if (attr.empty())
			return;

		// Find the equals after the attribute name
		size_t equalsPos = attr.findFirstOf('=');
		if (equalsPos == Common::String::npos)
			return;

		// Get the name, and find the quotes start
		Common::String name = Common::String(attr.c_str(), equalsPos);
		++equalsPos;
		while (equalsPos < attr.size() && Common::isSpace(attr[equalsPos]))
			++equalsPos;

		if (attr[equalsPos] == '\'' && attr[equalsPos] != '"')
			return;

		// Find the end of the attribute
		size_t attrEnd = attr.findFirstOf(attr[equalsPos], equalsPos + 1);
		if (attrEnd == Common::String::npos)
			return;

		// Add the parsed attribute
		_attributes[name] = Common::String(attr.c_str() + equalsPos + 1, attr.c_str() + attrEnd);

		// Remove the parsed attribute
		attr = Common::String(attr.c_str() + attrEnd + 1);
	}
}

XMLNode *XMLNode::xmlParseFile(XMLTree *tree, const Common::String &fname) {
	const Common::String rootFile = tree->_filename;
	Common::String filename = Common::String(rootFile.c_str(), rootFile.findLastOf('/') + 1) + fname;

	Common::File f;
	if (!f.open(filename))
		error("Could not open xml file - %s", filename.c_str());

	// Read in the file contents
	char *buf = new char[f.size() + 1];
	f.read(buf, f.size());
	buf[f.size()] = '\0';
	Common::String text(buf, buf + f.size());
	delete[] buf;
	f.close();

	// Parse the sub-xml
	XMLNode *result = xmlParseDoc(tree, text);
	if (!result)
		error("Error passing xml - %s", fname.c_str());

	return result;
}

bool XMLNode::searchPairs(KeyTypeList &ktl, const Common::String &basekey,
                          const Common::String currkey, const unsigned int pos) {
	/* If our 'current key' is longer then the key we're serching for
	    we've obviously gone too deep in this branch, and we won't find
	    it here. */
	if ((currkey.size() <= basekey.size()) && (_id[0] != '!')) {
		/* If we've found it, return every key->value pair under this key,
		    then return true, since we've found the key we were looking for.*/
		if (basekey == currkey + _id) {
			for (Common::Array<XMLNode *>::iterator i = _nodeList.begin();
			        i != _nodeList.end(); ++i)
				if ((*i)->_id[0] != '!')
					(*i)->selectPairs(ktl, "");
			return true;
		}
		/* Else, keep searching for the key under it's subnodes */
		else
			for (Common::Array<XMLNode *>::iterator i = _nodeList.begin();
			        i != _nodeList.end(); ++i)
				if ((*i)->searchPairs(ktl, basekey, currkey + _id + '/', pos))
					return true;
	}
	return false;
}

/* Just adds every key->value pair under the this node to the ktl */
void XMLNode::selectPairs(KeyTypeList &ktl, const Common::String currkey) {
	ktl.push_back(KeyType(currkey + _id, _content));

	for (Common::Array<XMLNode *>::iterator i = _nodeList.begin();
	        i != _nodeList.end(); ++i) {
		(*i)->selectPairs(ktl, currkey + _id + '/');
	}
}

XMLNode *XMLNode::getPrior() const {
	const Common::Array<XMLNode *> &siblings = _parent->_nodeList;
	for (uint idx = 0; idx < siblings.size(); ++idx) {
		if (siblings[idx] == this)
			return (idx > 0) ? siblings[idx - 1] : nullptr;
	}

	return nullptr;
}

XMLNode *XMLNode::getNext() const {
	const Common::Array<XMLNode *> &siblings = _parent->_nodeList;
	for (uint idx = 0; idx < siblings.size(); ++idx) {
		if (siblings[idx] == this)
			return (idx < (siblings.size() - 1)) ? siblings[idx + 1] : nullptr;
	}

	return nullptr;
}

void XMLNode::freeDoc() {
	delete _tree;
}

} // End of namespace Shared
} // End of namespace Ultima