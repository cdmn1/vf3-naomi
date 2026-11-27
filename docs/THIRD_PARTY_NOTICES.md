Dreamcast-Projects/makeip replacement IP bootstrap template
==========================================================

The NAOMI VF3 chainload diagnostic embeds the 32 KiB replacement IP.BIN
template from:

  Repository: https://github.com/Dreamcast-Projects/makeip
  File:       rsrc/templates/ip.tmpl
  Revision:   146b310e2d03c93648f56df81cd27b3458639f5c
  Git blob:   9ce8f2a7d6cc444ce198ac28ceef223b9848d17a

The template is fetched at build time and verified against the pinned Git
blob hash. It is copied to 0x8C008000 before the retail 1ST_READ.BIN handoff,
so the expected Dreamcast IP.BIN address range is populated.

BSD 2-Clause License
Copyright (C) 2000, 2001, 2002, 2019, 2020 The KOS Team and contributors.
All rights reserved.

This code was contributed to KallistiOS (KOS) by Andress Antonio Barajas
(BBHoodsta). It was originally made by Marcus Comstedt (zeldin). Some portions
of code were made by Andrew Kieschnick (ADK/Napalm). Heavily updated by
SiZiOUS. Bootstrap replacement (IP.TMPL) was made by Jacob Alberty (LiENUS).

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
