// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cert_issuer_source_static.h"

namespace bssl {

CertIssuerSourceStatic::CertIssuerSourceStatic() = default;
CertIssuerSourceStatic::~CertIssuerSourceStatic() = default;

void CertIssuerSourceStatic::AddCert(
    std::shared_ptr<const ParsedCertificate> cert) {
  intermediates_.insert(std::make_pair(
      BytesAsStringView(cert->normalized_subject()), std::move(cert)));
}

void CertIssuerSourceStatic::Clear() { intermediates_.clear(); }

std::vector<std::shared_ptr<const ParsedCertificate>>
CertIssuerSourceStatic::Certs() const {
  std::vector<std::shared_ptr<const ParsedCertificate>> result;
  result.reserve(intermediates_.size());
  for (const auto& [key, cert] : intermediates_) {
    result.push_back(cert);
  }
  return result;
}

void CertIssuerSourceStatic::SyncGetIssuersOf(const ParsedCertificate *cert,
                                              ParsedCertificateList *issuers) {
  auto range =
      intermediates_.equal_range(BytesAsStringView(cert->normalized_issuer()));
  for (auto it = range.first; it != range.second; ++it) {
    issuers->push_back(it->second);
  }
}

void CertIssuerSourceStatic::AsyncGetIssuersOf(
    const ParsedCertificate *cert, std::unique_ptr<Request> *out_req) {
  // CertIssuerSourceStatic never returns asynchronous results.
  out_req->reset();
}

}  // namespace bssl
