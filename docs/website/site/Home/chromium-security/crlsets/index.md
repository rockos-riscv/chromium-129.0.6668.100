---
breadcrumbs:
- - /Home
  - Chromium
- - /Home/chromium-security
  - Chromium Security
page_name: crlsets
title: CRLSets
---

(This page is intended for Certificate Authorities who wish to know about
Chromium's certificate revocation behaviour.)

CRLSets ([background](https://www.imperialviolet.org/2012/02/05/crlsets.html))
are the primary means by which Chrome quickly blocks certificates in
emergency situations. As a secondary function, they can also contain some number
of non-emergency revocations. These latter revocations are obtained by crawling
CRLs published by CAs.

Online (i.e. OCSP and CRL) checks are not, generally, performed by Chrome. They
can be enabled by policy and, in some cases, the underlying system certificate
library always performs these checks no matter what Chromium does.

The Chromium source code that implements CRLSets is, of course,
[public](https://chromium.googlesource.com/chromium/src/+/HEAD/net/cert/crl_set.cc).
But the process by which they are generated is not.

We crawl CRLs disclosed to CCADB and (for intermediates) those discovered via
Certificate Transparency.  CRLs on the list are fetched infrequently (at most
once every few hours) and verified against the correct signing certificate for
that CRL. A subset of the certificates identified as revoked on these CRLs are
included in the current CRLSet.

The current CRLSet can be fetched and dumped out using the code at
<https://github.com/agl/crlset-tools>.

The version of CRLSet being used by Chrome can be inspected by navigating to
chrome://components:

[<img alt="image"
src="/Home/chromium-security/crlsets/CRLSetComponents.png">](/Home/chromium-security/crlsets/CRLSetComponents.png)
