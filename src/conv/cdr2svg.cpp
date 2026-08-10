/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * cdr2svg - standalone CorelDRAW (CDR/CMX) to SVG converter for the
 * Inkscape CDR import extension. Built from vendored libcdr/librevenge
 * sources; produces one standalone SVG document on stdout.
 *
 * libcdr renders each page of a document into a separate SVG fragment.
 * By default we stitch them all back together into a single SVG laid out
 * left to right, describing the page boxes with the <inkscape:page>
 * elements Inkscape 1.2 and later use for multi-page documents.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>
#include <librevenge-stream/librevenge-stream.h>
#include <libcdr/libcdr.h>

#ifndef VERSION
#define VERSION "0.2.0"
#endif

namespace
{

/* Space left between pages, in user units (1/72 in), when laying a
   multi-page document out on the canvas. */
const double PAGE_GAP = 20.0;

struct Page
{
  Page() : content(), width(0.0), height(0.0), x(0.0) {}

  std::string content; // everything between <svg ...> and </svg>
  double width;        // user units (1/72 in)
  double height;
  double x;            // placement of the page in the merged document
};

int printUsage()
{
  fprintf(stderr, "cdr2svg " VERSION " - convert CorelDRAW documents to SVG\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: cdr2svg [OPTIONS] FILE\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Writes every page of the document as a single multi-page SVG,\n");
  fprintf(stderr, "pages laid out left to right.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "\t--all        output every page (default)\n");
  fprintf(stderr, "\t--page N     output only page N (1-based)\n");
  fprintf(stderr, "\t--no-clip    keep content parked outside the page box\n");
  fprintf(stderr, "\t             (it may then overlap the next page)\n");
  fprintf(stderr, "\t--pages      print the number of pages and exit\n");
  fprintf(stderr, "\t--version    print version and exit\n");
  fprintf(stderr, "\t--help       show this message\n");
  return 1;
}

/* Offset just past the '>' closing the tag that starts at `pos`, skipping
   over any '>' inside quoted attribute values. */
size_t findTagEnd(const std::string &s, size_t pos)
{
  char quote = 0;
  for (size_t i = pos; i < s.size(); ++i)
  {
    const char c = s[i];
    if (quote)
    {
      if (c == quote)
        quote = 0;
    }
    else if (c == '"' || c == '\'')
      quote = c;
    else if (c == '>')
      return i + 1;
  }
  return std::string::npos;
}

bool getAttribute(const std::string &tag, const std::string &name, std::string &value)
{
  for (size_t i = tag.find(name); i != std::string::npos; i = tag.find(name, i + name.size()))
  {
    if (i == 0 || !isspace((unsigned char)tag[i - 1]))
      continue;
    size_t j = i + name.size();
    while (j < tag.size() && isspace((unsigned char)tag[j]))
      ++j;
    if (j >= tag.size() || tag[j] != '=')
      continue;
    ++j;
    while (j < tag.size() && isspace((unsigned char)tag[j]))
      ++j;
    if (j >= tag.size() || (tag[j] != '"' && tag[j] != '\''))
      continue;
    const char quote = tag[j++];
    const size_t end = tag.find(quote, j);
    if (end == std::string::npos)
      return false;
    value.assign(tag, j, end - j);
    return true;
  }
  return false;
}

/* Parse an SVG length into user units (1/72 in). */
bool parseLength(const std::string &s, double &value)
{
  char *end = nullptr;
  const double number = strtod(s.c_str(), &end);
  if (!end || end == s.c_str())
    return false;
  while (*end && isspace((unsigned char)*end))
    ++end;

  double factor = 1.0;
  const std::string unit(end);
  if (unit.empty() || unit == "px" || unit == "pt")
    factor = 1.0;
  else if (unit == "in")
    factor = 72.0;
  else if (unit == "pc")
    factor = 12.0;
  else if (unit == "mm")
    factor = 72.0 / 25.4;
  else if (unit == "cm")
    factor = 72.0 / 2.54;
  else
    return false;

  value = number * factor;
  return value > 0.0;
}

/* Split one of librevenge's per-page SVG documents into its page geometry
   and its body. */
bool splitPage(const std::string &svg, Page &page)
{
  const size_t start = svg.find("<svg");
  if (start == std::string::npos)
    return false;
  const size_t tagEnd = findTagEnd(svg, start);
  if (tagEnd == std::string::npos)
    return false;
  const size_t close = svg.rfind("</svg>");
  if (close == std::string::npos || close < tagEnd)
    return false;

  const std::string tag(svg, start, tagEnd - start);
  page.content.assign(svg, tagEnd, close - tagEnd);

  std::string value;
  if (getAttribute(tag, "viewBox", value))
  {
    for (size_t i = 0; i < value.size(); ++i)
    {
      if (value[i] == ',')
        value[i] = ' ';
    }
    std::istringstream in(value);
    double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
    if ((in >> x >> y >> w >> h) && w > 0.0 && h > 0.0)
    {
      page.width = w;
      page.height = h;
    }
  }
  if (page.width <= 0.0 && getAttribute(tag, "width", value))
    parseLength(value, page.width);
  if (page.height <= 0.0 && getAttribute(tag, "height", value))
    parseLength(value, page.height);
  if (page.width <= 0.0)
    page.width = 612.0;
  if (page.height <= 0.0)
    page.height = 792.0;

  return true;
}

void collectIds(const std::string &content, std::vector<std::string> &ids)
{
  std::set<std::string> local;
  for (size_t i = content.find("id=\""); i != std::string::npos; i = content.find("id=\"", i))
  {
    if (i == 0 || !isspace((unsigned char)content[i - 1]))
    {
      i += 4;
      continue;
    }
    const size_t begin = i + 4;
    const size_t end = content.find('"', begin);
    if (end == std::string::npos)
      break;
    const std::string id(content, begin, end - begin);
    if (local.insert(id).second)
      ids.push_back(id);
    i = end + 1;
  }
}

void replaceAll(std::string &s, const std::string &from, const std::string &to)
{
  for (size_t pos = s.find(from); pos != std::string::npos; pos = s.find(from, pos + to.size()))
    s.replace(pos, from.size(), to);
}

/* Merging pages puts everything in one id namespace. librevenge numbers its
   gradient, pattern and marker ids monotonically across the whole document,
   but layer ids come from the file and can repeat between pages, so rename
   whatever we have already seen and fix up the references to it. */
void uniquifyIds(Page &page, unsigned number, std::set<std::string> &seen)
{
  std::vector<std::string> ids;
  collectIds(page.content, ids);

  for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++it)
  {
    const std::string &id = *it;
    if (seen.insert(id).second)
      continue;

    std::ostringstream candidate;
    candidate << id << "-" << number;
    std::string renamed = candidate.str();
    for (unsigned n = 2; !seen.insert(renamed).second; ++n)
    {
      std::ostringstream alternative;
      alternative << id << "-" << number << "-" << n;
      renamed = alternative.str();
    }

    replaceAll(page.content, "id=\"" + id + "\"", "id=\"" + renamed + "\"");
    replaceAll(page.content, "url(#" + id + ")", "url(#" + renamed + ")");
    replaceAll(page.content, "\"#" + id + "\"", "\"#" + renamed + "\"");
  }
}

std::string number(double value)
{
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%.4f", value);
  return buffer;
}

void writeMultiPage(std::vector<Page> &pages, bool clip)
{
  double offset = 0.0;
  for (size_t i = 0; i < pages.size(); ++i)
  {
    pages[i].x = offset;
    offset += pages[i].width + PAGE_GAP;
  }

  /* Inkscape ties the first page to the SVG viewport: whatever the first
     <inkscape:page> says, it is rewritten to the document's width/height on
     load. So the viewport has to describe page 1, and the remaining pages
     live on the canvas beyond it. That is how Inkscape saves its own
     multi-page documents. */
  const double width = pages[0].width;
  const double height = pages[0].height;

  std::ostringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
  out << "<svg version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\""
      << " xmlns:xlink=\"http://www.w3.org/1999/xlink\""
      << " xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\""
      << " xmlns:sodipodi=\"http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd\""
      << " width=\"" << number(width / 72.0) << "in\""
      << " height=\"" << number(height / 72.0) << "in\""
      << " viewBox=\"0 0 " << number(width) << " " << number(height) << "\" >\n";

  out << "<sodipodi:namedview id=\"namedview\" inkscape:document-units=\"in\">\n";
  for (size_t i = 0; i < pages.size(); ++i)
  {
    out << "  <inkscape:page x=\"" << number(pages[i].x) << "\" y=\"0\""
        << " width=\"" << number(pages[i].width) << "\""
        << " height=\"" << number(pages[i].height) << "\""
        << " id=\"page" << (i + 1) << "\" />\n";
  }
  out << "</sodipodi:namedview>\n";

  /* CorelDRAW keeps a shared desktop area around the pages, and objects
     parked there belong to a page's stream while sitting outside its page
     box. Lining the pages up side by side would drop that content on top of
     a neighbouring page, so clip each page to its own box the way Corel's
     own printing and PDF export do. --no-clip keeps the strays visible. */
  if (clip)
  {
    out << "<defs>\n";
    for (size_t i = 0; i < pages.size(); ++i)
    {
      out << "  <clipPath id=\"page-clip-" << (i + 1) << "\" clipPathUnits=\"userSpaceOnUse\">"
          << "<rect x=\"0\" y=\"0\""
          << " width=\"" << number(pages[i].width) << "\""
          << " height=\"" << number(pages[i].height) << "\" /></clipPath>\n";
    }
    out << "</defs>\n";
  }

  for (size_t i = 0; i < pages.size(); ++i)
  {
    out << "<g id=\"page-content-" << (i + 1) << "\" inkscape:label=\"Page " << (i + 1) << "\""
        << " transform=\"translate(" << number(pages[i].x) << ",0)\" >\n";
    if (clip)
      out << "<g id=\"page-clipped-" << (i + 1) << "\" clip-path=\"url(#page-clip-" << (i + 1) << ")\" >\n";
    out << pages[i].content;
    if (clip)
      out << "</g>\n";
    out << "</g>\n";
  }

  out << "</svg>\n";
  std::cout << out.str();
}

} // anonymous namespace

int main(int argc, char *argv[])
{
  const char *file = nullptr;
  bool countOnly = false;
  bool clip = true;
  long page = 0; // 0 means "every page"

  for (int i = 1; i < argc; ++i)
  {
    if (!strcmp(argv[i], "--help"))
      return printUsage();
    else if (!strcmp(argv[i], "--version"))
    {
      printf("cdr2svg " VERSION "\n");
      return 0;
    }
    else if (!strcmp(argv[i], "--pages"))
      countOnly = true;
    else if (!strcmp(argv[i], "--all"))
      page = 0;
    else if (!strcmp(argv[i], "--no-clip"))
      clip = false;
    else if (!strcmp(argv[i], "--page") && i + 1 < argc)
    {
      char *end = nullptr;
      page = strtol(argv[++i], &end, 10);
      if (!end || *end || page < 1)
      {
        fprintf(stderr, "ERROR: invalid page number '%s'\n", argv[i]);
        return 1;
      }
    }
    else if (!file && strncmp(argv[i], "--", 2))
      file = argv[i];
    else
      return printUsage();
  }

  if (!file)
    return printUsage();

  librevenge::RVNGFileStream input(file);
  librevenge::RVNGStringVector output;
  librevenge::RVNGSVGDrawingGenerator painter(output, "");

  if (libcdr::CDRDocument::isSupported(&input))
  {
    if (!libcdr::CDRDocument::parse(&input, &painter))
    {
      fprintf(stderr, "ERROR: parsing of CDR document failed\n");
      return 1;
    }
  }
  else if (libcdr::CMXDocument::isSupported(&input))
  {
    if (!libcdr::CMXDocument::parse(&input, &painter))
    {
      fprintf(stderr, "ERROR: parsing of CMX document failed\n");
      return 1;
    }
  }
  else
  {
    fprintf(stderr, "ERROR: unsupported file format (or the file is encrypted)\n");
    return 1;
  }

  if (output.empty())
  {
    fprintf(stderr, "ERROR: no SVG document generated\n");
    return 1;
  }

  if (countOnly)
  {
    printf("%u\n", output.size());
    return 0;
  }

  if ((unsigned long)page > (unsigned long)output.size())
  {
    fprintf(stderr, "ERROR: page %ld requested but document has only %u page(s)\n",
            page, output.size());
    return 1;
  }

  /* A single page needs none of the multi-page scaffolding. */
  if (page > 0 || output.size() == 1)
  {
    const unsigned index = page > 0 ? (unsigned)page - 1 : 0;
    std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
    std::cout << output[index].cstr() << std::endl;
    return 0;
  }

  std::vector<Page> pages;
  std::set<std::string> seen;
  /* Reserve the ids the wrapper itself uses. */
  seen.insert("namedview");
  for (unsigned i = 1; i <= output.size(); ++i)
  {
    std::ostringstream reserved;
    reserved << "page" << i;
    seen.insert(reserved.str());
    std::ostringstream group;
    group << "page-content-" << i;
    seen.insert(group.str());
    std::ostringstream clipped;
    clipped << "page-clipped-" << i;
    seen.insert(clipped.str());
    std::ostringstream clipPath;
    clipPath << "page-clip-" << i;
    seen.insert(clipPath.str());
  }

  for (unsigned i = 0; i < output.size(); ++i)
  {
    Page parsed;
    if (!splitPage(output[i].cstr(), parsed))
    {
      fprintf(stderr, "ERROR: could not parse the SVG generated for page %u\n", i + 1);
      return 1;
    }
    uniquifyIds(parsed, i + 1, seen);
    pages.push_back(parsed);
  }

  writeMultiPage(pages, clip);

  return 0;
}
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
