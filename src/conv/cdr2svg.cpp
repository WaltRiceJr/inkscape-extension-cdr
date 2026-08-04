/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * cdr2svg - standalone CorelDRAW (CDR/CMX) to SVG converter for the
 * Inkscape CDR import extension. Built from vendored libcdr/librevenge
 * sources; produces one standalone SVG document on stdout.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <librevenge/librevenge.h>
#include <librevenge-stream/librevenge-stream.h>
#include <libcdr/libcdr.h>

#ifndef VERSION
#define VERSION "0.1.0"
#endif

namespace
{

int printUsage()
{
  fprintf(stderr, "cdr2svg " VERSION " - convert CorelDRAW documents to SVG\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: cdr2svg [OPTIONS] FILE\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "\t--page N     output page N (1-based, default 1)\n");
  fprintf(stderr, "\t--pages      print the number of pages and exit\n");
  fprintf(stderr, "\t--version    print version and exit\n");
  fprintf(stderr, "\t--help       show this message\n");
  return 1;
}

} // anonymous namespace

int main(int argc, char *argv[])
{
  const char *file = nullptr;
  bool countOnly = false;
  long page = 1;

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

  std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
  std::cout << output[(unsigned long)page - 1].cstr() << std::endl;

  return 0;
}
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
