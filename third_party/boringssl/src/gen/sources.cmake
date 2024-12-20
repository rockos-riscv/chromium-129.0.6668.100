# Copyright (c) 2024, Google Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Generated by go ./util/pregenerate. Do not edit manually.

set(
  BCM_SOURCES

  crypto/fipsmodule/bcm.c
)

set(
  BCM_INTERNAL_HEADERS

  crypto/fipsmodule/aes/aes.c
  crypto/fipsmodule/aes/aes_nohw.c
  crypto/fipsmodule/aes/key_wrap.c
  crypto/fipsmodule/aes/mode_wrappers.c
  crypto/fipsmodule/bn/add.c
  crypto/fipsmodule/bn/asm/x86_64-gcc.c
  crypto/fipsmodule/bn/bn.c
  crypto/fipsmodule/bn/bytes.c
  crypto/fipsmodule/bn/cmp.c
  crypto/fipsmodule/bn/ctx.c
  crypto/fipsmodule/bn/div.c
  crypto/fipsmodule/bn/div_extra.c
  crypto/fipsmodule/bn/exponentiation.c
  crypto/fipsmodule/bn/gcd.c
  crypto/fipsmodule/bn/gcd_extra.c
  crypto/fipsmodule/bn/generic.c
  crypto/fipsmodule/bn/jacobi.c
  crypto/fipsmodule/bn/montgomery.c
  crypto/fipsmodule/bn/montgomery_inv.c
  crypto/fipsmodule/bn/mul.c
  crypto/fipsmodule/bn/prime.c
  crypto/fipsmodule/bn/random.c
  crypto/fipsmodule/bn/rsaz_exp.c
  crypto/fipsmodule/bn/shift.c
  crypto/fipsmodule/bn/sqrt.c
  crypto/fipsmodule/cipher/aead.c
  crypto/fipsmodule/cipher/cipher.c
  crypto/fipsmodule/cipher/e_aes.c
  crypto/fipsmodule/cipher/e_aesccm.c
  crypto/fipsmodule/cmac/cmac.c
  crypto/fipsmodule/dh/check.c
  crypto/fipsmodule/dh/dh.c
  crypto/fipsmodule/digest/digest.c
  crypto/fipsmodule/digest/digests.c
  crypto/fipsmodule/digestsign/digestsign.c
  crypto/fipsmodule/ec/ec.c
  crypto/fipsmodule/ec/ec_key.c
  crypto/fipsmodule/ec/ec_montgomery.c
  crypto/fipsmodule/ec/felem.c
  crypto/fipsmodule/ec/oct.c
  crypto/fipsmodule/ec/p224-64.c
  crypto/fipsmodule/ec/p256-nistz.c
  crypto/fipsmodule/ec/p256.c
  crypto/fipsmodule/ec/scalar.c
  crypto/fipsmodule/ec/simple.c
  crypto/fipsmodule/ec/simple_mul.c
  crypto/fipsmodule/ec/util.c
  crypto/fipsmodule/ec/wnaf.c
  crypto/fipsmodule/ecdh/ecdh.c
  crypto/fipsmodule/ecdsa/ecdsa.c
  crypto/fipsmodule/hkdf/hkdf.c
  crypto/fipsmodule/hmac/hmac.c
  crypto/fipsmodule/md4/md4.c
  crypto/fipsmodule/md5/md5.c
  crypto/fipsmodule/modes/cbc.c
  crypto/fipsmodule/modes/cfb.c
  crypto/fipsmodule/modes/ctr.c
  crypto/fipsmodule/modes/gcm.c
  crypto/fipsmodule/modes/gcm_nohw.c
  crypto/fipsmodule/modes/ofb.c
  crypto/fipsmodule/modes/polyval.c
  crypto/fipsmodule/rand/ctrdrbg.c
  crypto/fipsmodule/rand/fork_detect.c
  crypto/fipsmodule/rand/rand.c
  crypto/fipsmodule/rand/urandom.c
  crypto/fipsmodule/rsa/blinding.c
  crypto/fipsmodule/rsa/padding.c
  crypto/fipsmodule/rsa/rsa.c
  crypto/fipsmodule/rsa/rsa_impl.c
  crypto/fipsmodule/self_check/fips.c
  crypto/fipsmodule/self_check/self_check.c
  crypto/fipsmodule/service_indicator/service_indicator.c
  crypto/fipsmodule/sha/sha1.c
  crypto/fipsmodule/sha/sha256.c
  crypto/fipsmodule/sha/sha512.c
  crypto/fipsmodule/tls/kdf.c
)

set(
  BCM_SOURCES_ASM

  gen/bcm/aesni-gcm-x86_64-apple.S
  gen/bcm/aesni-gcm-x86_64-linux.S
  gen/bcm/aesni-x86-apple.S
  gen/bcm/aesni-x86-linux.S
  gen/bcm/aesni-x86_64-apple.S
  gen/bcm/aesni-x86_64-linux.S
  gen/bcm/aesp8-ppc-linux.S
  gen/bcm/aesv8-armv7-linux.S
  gen/bcm/aesv8-armv8-apple.S
  gen/bcm/aesv8-armv8-linux.S
  gen/bcm/aesv8-armv8-win.S
  gen/bcm/aesv8-gcm-armv8-apple.S
  gen/bcm/aesv8-gcm-armv8-linux.S
  gen/bcm/aesv8-gcm-armv8-win.S
  gen/bcm/armv4-mont-linux.S
  gen/bcm/armv8-mont-apple.S
  gen/bcm/armv8-mont-linux.S
  gen/bcm/armv8-mont-win.S
  gen/bcm/bn-586-apple.S
  gen/bcm/bn-586-linux.S
  gen/bcm/bn-armv8-apple.S
  gen/bcm/bn-armv8-linux.S
  gen/bcm/bn-armv8-win.S
  gen/bcm/bsaes-armv7-linux.S
  gen/bcm/co-586-apple.S
  gen/bcm/co-586-linux.S
  gen/bcm/ghash-armv4-linux.S
  gen/bcm/ghash-neon-armv8-apple.S
  gen/bcm/ghash-neon-armv8-linux.S
  gen/bcm/ghash-neon-armv8-win.S
  gen/bcm/ghash-ssse3-x86-apple.S
  gen/bcm/ghash-ssse3-x86-linux.S
  gen/bcm/ghash-ssse3-x86_64-apple.S
  gen/bcm/ghash-ssse3-x86_64-linux.S
  gen/bcm/ghash-x86-apple.S
  gen/bcm/ghash-x86-linux.S
  gen/bcm/ghash-x86_64-apple.S
  gen/bcm/ghash-x86_64-linux.S
  gen/bcm/ghashp8-ppc-linux.S
  gen/bcm/ghashv8-armv7-linux.S
  gen/bcm/ghashv8-armv8-apple.S
  gen/bcm/ghashv8-armv8-linux.S
  gen/bcm/ghashv8-armv8-win.S
  gen/bcm/md5-586-apple.S
  gen/bcm/md5-586-linux.S
  gen/bcm/md5-x86_64-apple.S
  gen/bcm/md5-x86_64-linux.S
  gen/bcm/p256-armv8-asm-apple.S
  gen/bcm/p256-armv8-asm-linux.S
  gen/bcm/p256-armv8-asm-win.S
  gen/bcm/p256-x86_64-asm-apple.S
  gen/bcm/p256-x86_64-asm-linux.S
  gen/bcm/p256_beeu-armv8-asm-apple.S
  gen/bcm/p256_beeu-armv8-asm-linux.S
  gen/bcm/p256_beeu-armv8-asm-win.S
  gen/bcm/p256_beeu-x86_64-asm-apple.S
  gen/bcm/p256_beeu-x86_64-asm-linux.S
  gen/bcm/rdrand-x86_64-apple.S
  gen/bcm/rdrand-x86_64-linux.S
  gen/bcm/rsaz-avx2-apple.S
  gen/bcm/rsaz-avx2-linux.S
  gen/bcm/sha1-586-apple.S
  gen/bcm/sha1-586-linux.S
  gen/bcm/sha1-armv4-large-linux.S
  gen/bcm/sha1-armv8-apple.S
  gen/bcm/sha1-armv8-linux.S
  gen/bcm/sha1-armv8-win.S
  gen/bcm/sha1-x86_64-apple.S
  gen/bcm/sha1-x86_64-linux.S
  gen/bcm/sha256-586-apple.S
  gen/bcm/sha256-586-linux.S
  gen/bcm/sha256-armv4-linux.S
  gen/bcm/sha256-armv8-apple.S
  gen/bcm/sha256-armv8-linux.S
  gen/bcm/sha256-armv8-win.S
  gen/bcm/sha256-x86_64-apple.S
  gen/bcm/sha256-x86_64-linux.S
  gen/bcm/sha512-586-apple.S
  gen/bcm/sha512-586-linux.S
  gen/bcm/sha512-armv4-linux.S
  gen/bcm/sha512-armv8-apple.S
  gen/bcm/sha512-armv8-linux.S
  gen/bcm/sha512-armv8-win.S
  gen/bcm/sha512-x86_64-apple.S
  gen/bcm/sha512-x86_64-linux.S
  gen/bcm/vpaes-armv7-linux.S
  gen/bcm/vpaes-armv8-apple.S
  gen/bcm/vpaes-armv8-linux.S
  gen/bcm/vpaes-armv8-win.S
  gen/bcm/vpaes-x86-apple.S
  gen/bcm/vpaes-x86-linux.S
  gen/bcm/vpaes-x86_64-apple.S
  gen/bcm/vpaes-x86_64-linux.S
  gen/bcm/x86-mont-apple.S
  gen/bcm/x86-mont-linux.S
  gen/bcm/x86_64-mont-apple.S
  gen/bcm/x86_64-mont-linux.S
  gen/bcm/x86_64-mont5-apple.S
  gen/bcm/x86_64-mont5-linux.S
  third_party/fiat/asm/fiat_p256_adx_mul.S
  third_party/fiat/asm/fiat_p256_adx_sqr.S
)

set(
  BCM_SOURCES_NASM

  gen/bcm/aesni-gcm-x86_64-win.asm
  gen/bcm/aesni-x86-win.asm
  gen/bcm/aesni-x86_64-win.asm
  gen/bcm/bn-586-win.asm
  gen/bcm/co-586-win.asm
  gen/bcm/ghash-ssse3-x86-win.asm
  gen/bcm/ghash-ssse3-x86_64-win.asm
  gen/bcm/ghash-x86-win.asm
  gen/bcm/ghash-x86_64-win.asm
  gen/bcm/md5-586-win.asm
  gen/bcm/md5-x86_64-win.asm
  gen/bcm/p256-x86_64-asm-win.asm
  gen/bcm/p256_beeu-x86_64-asm-win.asm
  gen/bcm/rdrand-x86_64-win.asm
  gen/bcm/rsaz-avx2-win.asm
  gen/bcm/sha1-586-win.asm
  gen/bcm/sha1-x86_64-win.asm
  gen/bcm/sha256-586-win.asm
  gen/bcm/sha256-x86_64-win.asm
  gen/bcm/sha512-586-win.asm
  gen/bcm/sha512-x86_64-win.asm
  gen/bcm/vpaes-x86-win.asm
  gen/bcm/vpaes-x86_64-win.asm
  gen/bcm/x86-mont-win.asm
  gen/bcm/x86_64-mont-win.asm
  gen/bcm/x86_64-mont5-win.asm
)

set(
  BSSL_SOURCES

  tool/args.cc
  tool/ciphers.cc
  tool/client.cc
  tool/const.cc
  tool/digest.cc
  tool/fd.cc
  tool/file.cc
  tool/generate_ech.cc
  tool/generate_ed25519.cc
  tool/genrsa.cc
  tool/pkcs12.cc
  tool/rand.cc
  tool/server.cc
  tool/sign.cc
  tool/speed.cc
  tool/tool.cc
  tool/transport_common.cc
)

set(
  BSSL_INTERNAL_HEADERS

  tool/internal.h
  tool/transport_common.h
)

set(
  CRYPTO_SOURCES

  crypto/asn1/a_bitstr.c
  crypto/asn1/a_bool.c
  crypto/asn1/a_d2i_fp.c
  crypto/asn1/a_dup.c
  crypto/asn1/a_gentm.c
  crypto/asn1/a_i2d_fp.c
  crypto/asn1/a_int.c
  crypto/asn1/a_mbstr.c
  crypto/asn1/a_object.c
  crypto/asn1/a_octet.c
  crypto/asn1/a_strex.c
  crypto/asn1/a_strnid.c
  crypto/asn1/a_time.c
  crypto/asn1/a_type.c
  crypto/asn1/a_utctm.c
  crypto/asn1/asn1_lib.c
  crypto/asn1/asn1_par.c
  crypto/asn1/asn_pack.c
  crypto/asn1/f_int.c
  crypto/asn1/f_string.c
  crypto/asn1/posix_time.c
  crypto/asn1/tasn_dec.c
  crypto/asn1/tasn_enc.c
  crypto/asn1/tasn_fre.c
  crypto/asn1/tasn_new.c
  crypto/asn1/tasn_typ.c
  crypto/asn1/tasn_utl.c
  crypto/base64/base64.c
  crypto/bio/bio.c
  crypto/bio/bio_mem.c
  crypto/bio/connect.c
  crypto/bio/errno.c
  crypto/bio/fd.c
  crypto/bio/file.c
  crypto/bio/hexdump.c
  crypto/bio/pair.c
  crypto/bio/printf.c
  crypto/bio/socket.c
  crypto/bio/socket_helper.c
  crypto/blake2/blake2.c
  crypto/bn_extra/bn_asn1.c
  crypto/bn_extra/convert.c
  crypto/buf/buf.c
  crypto/bytestring/asn1_compat.c
  crypto/bytestring/ber.c
  crypto/bytestring/cbb.c
  crypto/bytestring/cbs.c
  crypto/bytestring/unicode.c
  crypto/chacha/chacha.c
  crypto/cipher_extra/cipher_extra.c
  crypto/cipher_extra/derive_key.c
  crypto/cipher_extra/e_aesctrhmac.c
  crypto/cipher_extra/e_aesgcmsiv.c
  crypto/cipher_extra/e_chacha20poly1305.c
  crypto/cipher_extra/e_des.c
  crypto/cipher_extra/e_null.c
  crypto/cipher_extra/e_rc2.c
  crypto/cipher_extra/e_rc4.c
  crypto/cipher_extra/e_tls.c
  crypto/cipher_extra/tls_cbc.c
  crypto/conf/conf.c
  crypto/cpu_aarch64_apple.c
  crypto/cpu_aarch64_fuchsia.c
  crypto/cpu_aarch64_linux.c
  crypto/cpu_aarch64_openbsd.c
  crypto/cpu_aarch64_sysreg.c
  crypto/cpu_aarch64_win.c
  crypto/cpu_arm_freebsd.c
  crypto/cpu_arm_linux.c
  crypto/cpu_intel.c
  crypto/cpu_ppc64le.c
  crypto/crypto.c
  crypto/curve25519/curve25519.c
  crypto/curve25519/curve25519_64_adx.c
  crypto/curve25519/spake25519.c
  crypto/des/des.c
  crypto/dh_extra/dh_asn1.c
  crypto/dh_extra/params.c
  crypto/digest_extra/digest_extra.c
  crypto/dilithium/dilithium.c
  crypto/dsa/dsa.c
  crypto/dsa/dsa_asn1.c
  crypto/ec_extra/ec_asn1.c
  crypto/ec_extra/ec_derive.c
  crypto/ec_extra/hash_to_curve.c
  crypto/ecdh_extra/ecdh_extra.c
  crypto/ecdsa_extra/ecdsa_asn1.c
  crypto/engine/engine.c
  crypto/err/err.c
  crypto/evp/evp.c
  crypto/evp/evp_asn1.c
  crypto/evp/evp_ctx.c
  crypto/evp/p_dh.c
  crypto/evp/p_dh_asn1.c
  crypto/evp/p_dsa_asn1.c
  crypto/evp/p_ec.c
  crypto/evp/p_ec_asn1.c
  crypto/evp/p_ed25519.c
  crypto/evp/p_ed25519_asn1.c
  crypto/evp/p_hkdf.c
  crypto/evp/p_rsa.c
  crypto/evp/p_rsa_asn1.c
  crypto/evp/p_x25519.c
  crypto/evp/p_x25519_asn1.c
  crypto/evp/pbkdf.c
  crypto/evp/print.c
  crypto/evp/scrypt.c
  crypto/evp/sign.c
  crypto/ex_data.c
  crypto/fipsmodule/fips_shared_support.c
  crypto/hpke/hpke.c
  crypto/hrss/hrss.c
  crypto/keccak/keccak.c
  crypto/kyber/kyber.c
  crypto/lhash/lhash.c
  crypto/mem.c
  crypto/obj/obj.c
  crypto/obj/obj_xref.c
  crypto/pem/pem_all.c
  crypto/pem/pem_info.c
  crypto/pem/pem_lib.c
  crypto/pem/pem_oth.c
  crypto/pem/pem_pk8.c
  crypto/pem/pem_pkey.c
  crypto/pem/pem_x509.c
  crypto/pem/pem_xaux.c
  crypto/pkcs7/pkcs7.c
  crypto/pkcs7/pkcs7_x509.c
  crypto/pkcs8/p5_pbev2.c
  crypto/pkcs8/pkcs8.c
  crypto/pkcs8/pkcs8_x509.c
  crypto/poly1305/poly1305.c
  crypto/poly1305/poly1305_arm.c
  crypto/poly1305/poly1305_vec.c
  crypto/pool/pool.c
  crypto/rand_extra/deterministic.c
  crypto/rand_extra/forkunsafe.c
  crypto/rand_extra/getentropy.c
  crypto/rand_extra/ios.c
  crypto/rand_extra/passive.c
  crypto/rand_extra/rand_extra.c
  crypto/rand_extra/trusty.c
  crypto/rand_extra/windows.c
  crypto/rc4/rc4.c
  crypto/refcount.c
  crypto/rsa_extra/rsa_asn1.c
  crypto/rsa_extra/rsa_crypt.c
  crypto/rsa_extra/rsa_print.c
  crypto/siphash/siphash.c
  crypto/spx/address.c
  crypto/spx/fors.c
  crypto/spx/merkle.c
  crypto/spx/spx.c
  crypto/spx/spx_util.c
  crypto/spx/thash.c
  crypto/spx/wots.c
  crypto/stack/stack.c
  crypto/thread.c
  crypto/thread_none.c
  crypto/thread_pthread.c
  crypto/thread_win.c
  crypto/trust_token/pmbtoken.c
  crypto/trust_token/trust_token.c
  crypto/trust_token/voprf.c
  crypto/x509/a_digest.c
  crypto/x509/a_sign.c
  crypto/x509/a_verify.c
  crypto/x509/algorithm.c
  crypto/x509/asn1_gen.c
  crypto/x509/by_dir.c
  crypto/x509/by_file.c
  crypto/x509/i2d_pr.c
  crypto/x509/name_print.c
  crypto/x509/policy.c
  crypto/x509/rsa_pss.c
  crypto/x509/t_crl.c
  crypto/x509/t_req.c
  crypto/x509/t_x509.c
  crypto/x509/t_x509a.c
  crypto/x509/v3_akey.c
  crypto/x509/v3_akeya.c
  crypto/x509/v3_alt.c
  crypto/x509/v3_bcons.c
  crypto/x509/v3_bitst.c
  crypto/x509/v3_conf.c
  crypto/x509/v3_cpols.c
  crypto/x509/v3_crld.c
  crypto/x509/v3_enum.c
  crypto/x509/v3_extku.c
  crypto/x509/v3_genn.c
  crypto/x509/v3_ia5.c
  crypto/x509/v3_info.c
  crypto/x509/v3_int.c
  crypto/x509/v3_lib.c
  crypto/x509/v3_ncons.c
  crypto/x509/v3_ocsp.c
  crypto/x509/v3_pcons.c
  crypto/x509/v3_pmaps.c
  crypto/x509/v3_prn.c
  crypto/x509/v3_purp.c
  crypto/x509/v3_skey.c
  crypto/x509/v3_utl.c
  crypto/x509/x509.c
  crypto/x509/x509_att.c
  crypto/x509/x509_cmp.c
  crypto/x509/x509_d2.c
  crypto/x509/x509_def.c
  crypto/x509/x509_ext.c
  crypto/x509/x509_lu.c
  crypto/x509/x509_obj.c
  crypto/x509/x509_req.c
  crypto/x509/x509_set.c
  crypto/x509/x509_trs.c
  crypto/x509/x509_txt.c
  crypto/x509/x509_v3.c
  crypto/x509/x509_vfy.c
  crypto/x509/x509_vpm.c
  crypto/x509/x509cset.c
  crypto/x509/x509name.c
  crypto/x509/x509rset.c
  crypto/x509/x509spki.c
  crypto/x509/x_algor.c
  crypto/x509/x_all.c
  crypto/x509/x_attrib.c
  crypto/x509/x_crl.c
  crypto/x509/x_exten.c
  crypto/x509/x_name.c
  crypto/x509/x_pubkey.c
  crypto/x509/x_req.c
  crypto/x509/x_sig.c
  crypto/x509/x_spki.c
  crypto/x509/x_val.c
  crypto/x509/x_x509.c
  crypto/x509/x_x509a.c
  gen/crypto/err_data.c
)

set(
  CRYPTO_HEADERS

  include/openssl/aead.h
  include/openssl/aes.h
  include/openssl/arm_arch.h
  include/openssl/asm_base.h
  include/openssl/asn1.h
  include/openssl/asn1_mac.h
  include/openssl/asn1t.h
  include/openssl/base.h
  include/openssl/base64.h
  include/openssl/bio.h
  include/openssl/blake2.h
  include/openssl/blowfish.h
  include/openssl/bn.h
  include/openssl/buf.h
  include/openssl/buffer.h
  include/openssl/bytestring.h
  include/openssl/cast.h
  include/openssl/chacha.h
  include/openssl/cipher.h
  include/openssl/cmac.h
  include/openssl/conf.h
  include/openssl/cpu.h
  include/openssl/crypto.h
  include/openssl/ctrdrbg.h
  include/openssl/curve25519.h
  include/openssl/des.h
  include/openssl/dh.h
  include/openssl/digest.h
  include/openssl/dsa.h
  include/openssl/e_os2.h
  include/openssl/ec.h
  include/openssl/ec_key.h
  include/openssl/ecdh.h
  include/openssl/ecdsa.h
  include/openssl/engine.h
  include/openssl/err.h
  include/openssl/evp.h
  include/openssl/evp_errors.h
  include/openssl/ex_data.h
  include/openssl/experimental/dilithium.h
  include/openssl/experimental/kyber.h
  include/openssl/experimental/spx.h
  include/openssl/hkdf.h
  include/openssl/hmac.h
  include/openssl/hpke.h
  include/openssl/hrss.h
  include/openssl/is_boringssl.h
  include/openssl/kdf.h
  include/openssl/lhash.h
  include/openssl/md4.h
  include/openssl/md5.h
  include/openssl/mem.h
  include/openssl/nid.h
  include/openssl/obj.h
  include/openssl/obj_mac.h
  include/openssl/objects.h
  include/openssl/opensslconf.h
  include/openssl/opensslv.h
  include/openssl/ossl_typ.h
  include/openssl/pem.h
  include/openssl/pkcs12.h
  include/openssl/pkcs7.h
  include/openssl/pkcs8.h
  include/openssl/poly1305.h
  include/openssl/pool.h
  include/openssl/posix_time.h
  include/openssl/rand.h
  include/openssl/rc4.h
  include/openssl/ripemd.h
  include/openssl/rsa.h
  include/openssl/safestack.h
  include/openssl/service_indicator.h
  include/openssl/sha.h
  include/openssl/siphash.h
  include/openssl/span.h
  include/openssl/stack.h
  include/openssl/target.h
  include/openssl/thread.h
  include/openssl/time.h
  include/openssl/trust_token.h
  include/openssl/type_check.h
  include/openssl/x509.h
  include/openssl/x509_vfy.h
  include/openssl/x509v3.h
  include/openssl/x509v3_errors.h
)

set(
  CRYPTO_INTERNAL_HEADERS

  crypto/asn1/internal.h
  crypto/bio/internal.h
  crypto/bytestring/internal.h
  crypto/chacha/internal.h
  crypto/cipher_extra/internal.h
  crypto/conf/internal.h
  crypto/cpu_arm_linux.h
  crypto/curve25519/curve25519_tables.h
  crypto/curve25519/internal.h
  crypto/des/internal.h
  crypto/dilithium/internal.h
  crypto/dsa/internal.h
  crypto/ec_extra/internal.h
  crypto/err/internal.h
  crypto/evp/internal.h
  crypto/fipsmodule/aes/internal.h
  crypto/fipsmodule/bn/internal.h
  crypto/fipsmodule/bn/rsaz_exp.h
  crypto/fipsmodule/cipher/internal.h
  crypto/fipsmodule/delocate.h
  crypto/fipsmodule/dh/internal.h
  crypto/fipsmodule/digest/internal.h
  crypto/fipsmodule/digest/md32_common.h
  crypto/fipsmodule/ec/builtin_curves.h
  crypto/fipsmodule/ec/internal.h
  crypto/fipsmodule/ec/p256-nistz-table.h
  crypto/fipsmodule/ec/p256-nistz.h
  crypto/fipsmodule/ec/p256_table.h
  crypto/fipsmodule/ecdsa/internal.h
  crypto/fipsmodule/md5/internal.h
  crypto/fipsmodule/modes/internal.h
  crypto/fipsmodule/rand/fork_detect.h
  crypto/fipsmodule/rand/getrandom_fillin.h
  crypto/fipsmodule/rand/internal.h
  crypto/fipsmodule/rsa/internal.h
  crypto/fipsmodule/service_indicator/internal.h
  crypto/fipsmodule/sha/internal.h
  crypto/fipsmodule/tls/internal.h
  crypto/hrss/internal.h
  crypto/internal.h
  crypto/keccak/internal.h
  crypto/kyber/internal.h
  crypto/lhash/internal.h
  crypto/obj/obj_dat.h
  crypto/pkcs7/internal.h
  crypto/pkcs8/internal.h
  crypto/poly1305/internal.h
  crypto/pool/internal.h
  crypto/rsa_extra/internal.h
  crypto/spx/address.h
  crypto/spx/fors.h
  crypto/spx/merkle.h
  crypto/spx/params.h
  crypto/spx/spx_util.h
  crypto/spx/thash.h
  crypto/spx/wots.h
  crypto/trust_token/internal.h
  crypto/x509/ext_dat.h
  crypto/x509/internal.h
  third_party/fiat/curve25519_32.h
  third_party/fiat/curve25519_64.h
  third_party/fiat/curve25519_64_adx.h
  third_party/fiat/curve25519_64_msvc.h
  third_party/fiat/p256_32.h
  third_party/fiat/p256_64.h
  third_party/fiat/p256_64_msvc.h
)

set(
  CRYPTO_SOURCES_ASM

  crypto/curve25519/asm/x25519-asm-arm.S
  crypto/hrss/asm/poly_rq_mul.S
  crypto/poly1305/poly1305_arm_asm.S
  gen/crypto/aes128gcmsiv-x86_64-apple.S
  gen/crypto/aes128gcmsiv-x86_64-linux.S
  gen/crypto/chacha-armv4-linux.S
  gen/crypto/chacha-armv8-apple.S
  gen/crypto/chacha-armv8-linux.S
  gen/crypto/chacha-armv8-win.S
  gen/crypto/chacha-x86-apple.S
  gen/crypto/chacha-x86-linux.S
  gen/crypto/chacha-x86_64-apple.S
  gen/crypto/chacha-x86_64-linux.S
  gen/crypto/chacha20_poly1305_armv8-apple.S
  gen/crypto/chacha20_poly1305_armv8-linux.S
  gen/crypto/chacha20_poly1305_armv8-win.S
  gen/crypto/chacha20_poly1305_x86_64-apple.S
  gen/crypto/chacha20_poly1305_x86_64-linux.S
  third_party/fiat/asm/fiat_curve25519_adx_mul.S
  third_party/fiat/asm/fiat_curve25519_adx_square.S
)

set(
  CRYPTO_SOURCES_NASM

  gen/crypto/aes128gcmsiv-x86_64-win.asm
  gen/crypto/chacha-x86-win.asm
  gen/crypto/chacha-x86_64-win.asm
  gen/crypto/chacha20_poly1305_x86_64-win.asm
)

set(
  CRYPTO_TEST_SOURCES

  crypto/abi_self_test.cc
  crypto/asn1/asn1_test.cc
  crypto/base64/base64_test.cc
  crypto/bio/bio_test.cc
  crypto/blake2/blake2_test.cc
  crypto/buf/buf_test.cc
  crypto/bytestring/bytestring_test.cc
  crypto/chacha/chacha_test.cc
  crypto/cipher_extra/aead_test.cc
  crypto/cipher_extra/cipher_test.cc
  crypto/compiler_test.cc
  crypto/conf/conf_test.cc
  crypto/constant_time_test.cc
  crypto/cpu_arm_linux_test.cc
  crypto/crypto_test.cc
  crypto/curve25519/ed25519_test.cc
  crypto/curve25519/spake25519_test.cc
  crypto/curve25519/x25519_test.cc
  crypto/dh_extra/dh_test.cc
  crypto/digest_extra/digest_test.cc
  crypto/dilithium/dilithium_test.cc
  crypto/dsa/dsa_test.cc
  crypto/ecdh_extra/ecdh_test.cc
  crypto/err/err_test.cc
  crypto/evp/evp_extra_test.cc
  crypto/evp/evp_test.cc
  crypto/evp/pbkdf_test.cc
  crypto/evp/scrypt_test.cc
  crypto/fipsmodule/aes/aes_test.cc
  crypto/fipsmodule/bn/bn_test.cc
  crypto/fipsmodule/cmac/cmac_test.cc
  crypto/fipsmodule/ec/ec_test.cc
  crypto/fipsmodule/ec/p256-nistz_test.cc
  crypto/fipsmodule/ec/p256_test.cc
  crypto/fipsmodule/ecdsa/ecdsa_test.cc
  crypto/fipsmodule/hkdf/hkdf_test.cc
  crypto/fipsmodule/md5/md5_test.cc
  crypto/fipsmodule/modes/gcm_test.cc
  crypto/fipsmodule/rand/ctrdrbg_test.cc
  crypto/fipsmodule/rand/fork_detect_test.cc
  crypto/fipsmodule/service_indicator/service_indicator_test.cc
  crypto/fipsmodule/sha/sha_test.cc
  crypto/hmac_extra/hmac_test.cc
  crypto/hpke/hpke_test.cc
  crypto/hrss/hrss_test.cc
  crypto/impl_dispatch_test.cc
  crypto/keccak/keccak_test.cc
  crypto/kyber/kyber_test.cc
  crypto/lhash/lhash_test.cc
  crypto/obj/obj_test.cc
  crypto/pem/pem_test.cc
  crypto/pkcs7/pkcs7_test.cc
  crypto/pkcs8/pkcs12_test.cc
  crypto/pkcs8/pkcs8_test.cc
  crypto/poly1305/poly1305_test.cc
  crypto/pool/pool_test.cc
  crypto/rand_extra/getentropy_test.cc
  crypto/rand_extra/rand_test.cc
  crypto/refcount_test.cc
  crypto/rsa_extra/rsa_test.cc
  crypto/self_test.cc
  crypto/siphash/siphash_test.cc
  crypto/spx/spx_test.cc
  crypto/stack/stack_test.cc
  crypto/test/gtest_main.cc
  crypto/thread_test.cc
  crypto/trust_token/trust_token_test.cc
  crypto/x509/tab_test.cc
  crypto/x509/x509_test.cc
  crypto/x509/x509_time_test.cc
)

set(
  CRYPTO_TEST_DATA

  crypto/blake2/blake2b256_tests.txt
  crypto/curve25519/ed25519_tests.txt
  crypto/dilithium/dilithium_tests.txt
  crypto/dilithium/edge_cases_draft_dilithium3_sign.txt
  crypto/dilithium/edge_cases_draft_dilithium3_verify.txt
  crypto/ecdh_extra/ecdh_tests.txt
  crypto/evp/evp_tests.txt
  crypto/evp/scrypt_tests.txt
  crypto/fipsmodule/aes/aes_tests.txt
  crypto/fipsmodule/bn/test/exp_tests.txt
  crypto/fipsmodule/bn/test/gcd_tests.txt
  crypto/fipsmodule/bn/test/miller_rabin_tests.txt
  crypto/fipsmodule/bn/test/mod_exp_tests.txt
  crypto/fipsmodule/bn/test/mod_inv_tests.txt
  crypto/fipsmodule/bn/test/mod_mul_tests.txt
  crypto/fipsmodule/bn/test/mod_sqrt_tests.txt
  crypto/fipsmodule/bn/test/product_tests.txt
  crypto/fipsmodule/bn/test/quotient_tests.txt
  crypto/fipsmodule/bn/test/shift_tests.txt
  crypto/fipsmodule/bn/test/sum_tests.txt
  crypto/fipsmodule/cmac/cavp_3des_cmac_tests.txt
  crypto/fipsmodule/cmac/cavp_aes128_cmac_tests.txt
  crypto/fipsmodule/cmac/cavp_aes192_cmac_tests.txt
  crypto/fipsmodule/cmac/cavp_aes256_cmac_tests.txt
  crypto/fipsmodule/ec/ec_scalar_base_mult_tests.txt
  crypto/fipsmodule/ec/p256-nistz_tests.txt
  crypto/fipsmodule/ecdsa/ecdsa_sign_tests.txt
  crypto/fipsmodule/ecdsa/ecdsa_verify_tests.txt
  crypto/fipsmodule/modes/gcm_tests.txt
  crypto/fipsmodule/rand/ctrdrbg_vectors.txt
  crypto/hmac_extra/hmac_tests.txt
  crypto/hpke/hpke_test_vectors.txt
  crypto/keccak/keccak_tests.txt
  crypto/kyber/kyber_tests.txt
  crypto/pkcs8/test/bad1.p12
  crypto/pkcs8/test/bad2.p12
  crypto/pkcs8/test/bad3.p12
  crypto/pkcs8/test/empty_password.p12
  crypto/pkcs8/test/empty_password_ber.p12
  crypto/pkcs8/test/empty_password_ber_nested.p12
  crypto/pkcs8/test/no_encryption.p12
  crypto/pkcs8/test/nss.p12
  crypto/pkcs8/test/null_password.p12
  crypto/pkcs8/test/openssl.p12
  crypto/pkcs8/test/pbes2_sha1.p12
  crypto/pkcs8/test/pbes2_sha256.p12
  crypto/pkcs8/test/unicode_password.p12
  crypto/pkcs8/test/windows.p12
  crypto/poly1305/poly1305_tests.txt
  crypto/siphash/siphash_tests.txt
  crypto/spx/spx_tests.txt
  crypto/spx/spx_tests_deterministic.txt
  crypto/x509/test/basic_constraints_ca.pem
  crypto/x509/test/basic_constraints_ca_pathlen_0.pem
  crypto/x509/test/basic_constraints_ca_pathlen_1.pem
  crypto/x509/test/basic_constraints_ca_pathlen_10.pem
  crypto/x509/test/basic_constraints_leaf.pem
  crypto/x509/test/basic_constraints_none.pem
  crypto/x509/test/invalid_extension_intermediate.pem
  crypto/x509/test/invalid_extension_intermediate_authority_key_identifier.pem
  crypto/x509/test/invalid_extension_intermediate_basic_constraints.pem
  crypto/x509/test/invalid_extension_intermediate_ext_key_usage.pem
  crypto/x509/test/invalid_extension_intermediate_key_usage.pem
  crypto/x509/test/invalid_extension_intermediate_name_constraints.pem
  crypto/x509/test/invalid_extension_intermediate_subject_alt_name.pem
  crypto/x509/test/invalid_extension_intermediate_subject_key_identifier.pem
  crypto/x509/test/invalid_extension_leaf.pem
  crypto/x509/test/invalid_extension_leaf_authority_key_identifier.pem
  crypto/x509/test/invalid_extension_leaf_basic_constraints.pem
  crypto/x509/test/invalid_extension_leaf_ext_key_usage.pem
  crypto/x509/test/invalid_extension_leaf_key_usage.pem
  crypto/x509/test/invalid_extension_leaf_name_constraints.pem
  crypto/x509/test/invalid_extension_leaf_subject_alt_name.pem
  crypto/x509/test/invalid_extension_leaf_subject_key_identifier.pem
  crypto/x509/test/invalid_extension_root.pem
  crypto/x509/test/invalid_extension_root_authority_key_identifier.pem
  crypto/x509/test/invalid_extension_root_basic_constraints.pem
  crypto/x509/test/invalid_extension_root_ext_key_usage.pem
  crypto/x509/test/invalid_extension_root_key_usage.pem
  crypto/x509/test/invalid_extension_root_name_constraints.pem
  crypto/x509/test/invalid_extension_root_subject_alt_name.pem
  crypto/x509/test/invalid_extension_root_subject_key_identifier.pem
  crypto/x509/test/many_constraints.pem
  crypto/x509/test/many_names1.pem
  crypto/x509/test/many_names2.pem
  crypto/x509/test/many_names3.pem
  crypto/x509/test/policy_intermediate.pem
  crypto/x509/test/policy_intermediate_any.pem
  crypto/x509/test/policy_intermediate_duplicate.pem
  crypto/x509/test/policy_intermediate_invalid.pem
  crypto/x509/test/policy_intermediate_mapped.pem
  crypto/x509/test/policy_intermediate_mapped_any.pem
  crypto/x509/test/policy_intermediate_mapped_oid3.pem
  crypto/x509/test/policy_intermediate_require.pem
  crypto/x509/test/policy_intermediate_require1.pem
  crypto/x509/test/policy_intermediate_require2.pem
  crypto/x509/test/policy_intermediate_require_duplicate.pem
  crypto/x509/test/policy_intermediate_require_no_policies.pem
  crypto/x509/test/policy_leaf.pem
  crypto/x509/test/policy_leaf_any.pem
  crypto/x509/test/policy_leaf_duplicate.pem
  crypto/x509/test/policy_leaf_invalid.pem
  crypto/x509/test/policy_leaf_none.pem
  crypto/x509/test/policy_leaf_oid1.pem
  crypto/x509/test/policy_leaf_oid2.pem
  crypto/x509/test/policy_leaf_oid3.pem
  crypto/x509/test/policy_leaf_oid4.pem
  crypto/x509/test/policy_leaf_oid5.pem
  crypto/x509/test/policy_leaf_require.pem
  crypto/x509/test/policy_leaf_require1.pem
  crypto/x509/test/policy_root.pem
  crypto/x509/test/policy_root2.pem
  crypto/x509/test/policy_root_cross_inhibit_mapping.pem
  crypto/x509/test/pss_sha1.pem
  crypto/x509/test/pss_sha1_explicit.pem
  crypto/x509/test/pss_sha1_mgf1_syntax_error.pem
  crypto/x509/test/pss_sha224.pem
  crypto/x509/test/pss_sha256.pem
  crypto/x509/test/pss_sha256_explicit_trailer.pem
  crypto/x509/test/pss_sha256_mgf1_sha384.pem
  crypto/x509/test/pss_sha256_mgf1_syntax_error.pem
  crypto/x509/test/pss_sha256_omit_nulls.pem
  crypto/x509/test/pss_sha256_salt31.pem
  crypto/x509/test/pss_sha256_salt_overflow.pem
  crypto/x509/test/pss_sha256_unknown_mgf.pem
  crypto/x509/test/pss_sha256_wrong_trailer.pem
  crypto/x509/test/pss_sha384.pem
  crypto/x509/test/pss_sha512.pem
  crypto/x509/test/some_names1.pem
  crypto/x509/test/some_names2.pem
  crypto/x509/test/some_names3.pem
  crypto/x509/test/trailing_data_leaf_authority_key_identifier.pem
  crypto/x509/test/trailing_data_leaf_basic_constraints.pem
  crypto/x509/test/trailing_data_leaf_ext_key_usage.pem
  crypto/x509/test/trailing_data_leaf_key_usage.pem
  crypto/x509/test/trailing_data_leaf_name_constraints.pem
  crypto/x509/test/trailing_data_leaf_subject_alt_name.pem
  crypto/x509/test/trailing_data_leaf_subject_key_identifier.pem
  third_party/wycheproof_testvectors/aes_cbc_pkcs5_test.txt
  third_party/wycheproof_testvectors/aes_cmac_test.txt
  third_party/wycheproof_testvectors/aes_gcm_siv_test.txt
  third_party/wycheproof_testvectors/aes_gcm_test.txt
  third_party/wycheproof_testvectors/chacha20_poly1305_test.txt
  third_party/wycheproof_testvectors/dsa_test.txt
  third_party/wycheproof_testvectors/ecdh_secp224r1_test.txt
  third_party/wycheproof_testvectors/ecdh_secp256r1_test.txt
  third_party/wycheproof_testvectors/ecdh_secp384r1_test.txt
  third_party/wycheproof_testvectors/ecdh_secp521r1_test.txt
  third_party/wycheproof_testvectors/ecdsa_secp224r1_sha224_test.txt
  third_party/wycheproof_testvectors/ecdsa_secp224r1_sha256_test.txt
  third_party/wycheproof_testvectors/ecdsa_secp224r1_sha512_test.txt
  third_party/wycheproof_testvectors/ecdsa_secp256r1_sha256_test.txt
  third_party/wycheproof_testvectors/ecdsa_secp256r1_sha512_test.txt
  third_party/wycheproof_testvectors/ecdsa_secp384r1_sha384_test.txt
  third_party/wycheproof_testvectors/ecdsa_secp384r1_sha512_test.txt
  third_party/wycheproof_testvectors/ecdsa_secp521r1_sha512_test.txt
  third_party/wycheproof_testvectors/eddsa_test.txt
  third_party/wycheproof_testvectors/hkdf_sha1_test.txt
  third_party/wycheproof_testvectors/hkdf_sha256_test.txt
  third_party/wycheproof_testvectors/hkdf_sha384_test.txt
  third_party/wycheproof_testvectors/hkdf_sha512_test.txt
  third_party/wycheproof_testvectors/hmac_sha1_test.txt
  third_party/wycheproof_testvectors/hmac_sha224_test.txt
  third_party/wycheproof_testvectors/hmac_sha256_test.txt
  third_party/wycheproof_testvectors/hmac_sha384_test.txt
  third_party/wycheproof_testvectors/hmac_sha512_test.txt
  third_party/wycheproof_testvectors/kw_test.txt
  third_party/wycheproof_testvectors/kwp_test.txt
  third_party/wycheproof_testvectors/primality_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_2048_sha1_mgf1sha1_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_2048_sha224_mgf1sha1_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_2048_sha224_mgf1sha224_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_2048_sha256_mgf1sha1_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_2048_sha256_mgf1sha256_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_2048_sha384_mgf1sha1_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_2048_sha384_mgf1sha384_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_2048_sha512_mgf1sha1_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_2048_sha512_mgf1sha512_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_3072_sha256_mgf1sha1_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_3072_sha256_mgf1sha256_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_3072_sha512_mgf1sha1_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_3072_sha512_mgf1sha512_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_4096_sha256_mgf1sha1_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_4096_sha256_mgf1sha256_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_4096_sha512_mgf1sha1_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_4096_sha512_mgf1sha512_test.txt
  third_party/wycheproof_testvectors/rsa_oaep_misc_test.txt
  third_party/wycheproof_testvectors/rsa_pkcs1_2048_test.txt
  third_party/wycheproof_testvectors/rsa_pkcs1_3072_test.txt
  third_party/wycheproof_testvectors/rsa_pkcs1_4096_test.txt
  third_party/wycheproof_testvectors/rsa_pss_2048_sha1_mgf1_20_test.txt
  third_party/wycheproof_testvectors/rsa_pss_2048_sha256_mgf1_0_test.txt
  third_party/wycheproof_testvectors/rsa_pss_2048_sha256_mgf1_32_test.txt
  third_party/wycheproof_testvectors/rsa_pss_3072_sha256_mgf1_32_test.txt
  third_party/wycheproof_testvectors/rsa_pss_4096_sha256_mgf1_32_test.txt
  third_party/wycheproof_testvectors/rsa_pss_4096_sha512_mgf1_32_test.txt
  third_party/wycheproof_testvectors/rsa_pss_misc_test.txt
  third_party/wycheproof_testvectors/rsa_sig_gen_misc_test.txt
  third_party/wycheproof_testvectors/rsa_signature_2048_sha224_test.txt
  third_party/wycheproof_testvectors/rsa_signature_2048_sha256_test.txt
  third_party/wycheproof_testvectors/rsa_signature_2048_sha384_test.txt
  third_party/wycheproof_testvectors/rsa_signature_2048_sha512_test.txt
  third_party/wycheproof_testvectors/rsa_signature_3072_sha256_test.txt
  third_party/wycheproof_testvectors/rsa_signature_3072_sha384_test.txt
  third_party/wycheproof_testvectors/rsa_signature_3072_sha512_test.txt
  third_party/wycheproof_testvectors/rsa_signature_4096_sha384_test.txt
  third_party/wycheproof_testvectors/rsa_signature_4096_sha512_test.txt
  third_party/wycheproof_testvectors/rsa_signature_test.txt
  third_party/wycheproof_testvectors/x25519_test.txt
  third_party/wycheproof_testvectors/xchacha20_poly1305_test.txt
)

set(
  DECREPIT_SOURCES

  decrepit/bio/base64_bio.c
  decrepit/blowfish/blowfish.c
  decrepit/cast/cast.c
  decrepit/cast/cast_tables.c
  decrepit/cfb/cfb.c
  decrepit/des/cfb64ede.c
  decrepit/dh/dh_decrepit.c
  decrepit/dsa/dsa_decrepit.c
  decrepit/evp/dss1.c
  decrepit/evp/evp_do_all.c
  decrepit/obj/obj_decrepit.c
  decrepit/rc4/rc4_decrepit.c
  decrepit/ripemd/ripemd.c
  decrepit/rsa/rsa_decrepit.c
  decrepit/ssl/ssl_decrepit.c
  decrepit/x509/x509_decrepit.c
  decrepit/xts/xts.c
)

set(
  DECREPIT_INTERNAL_HEADERS

  decrepit/cast/internal.h
  decrepit/macros.h
)

set(
  DECREPIT_TEST_SOURCES

  crypto/test/gtest_main.cc
  decrepit/blowfish/blowfish_test.cc
  decrepit/cast/cast_test.cc
  decrepit/cfb/cfb_test.cc
  decrepit/des/des_test.cc
  decrepit/evp/evp_test.cc
  decrepit/ripemd/ripemd_test.cc
  decrepit/xts/xts_test.cc
)

set(
  PKI_SOURCES

  pki/cert_error_id.cc
  pki/cert_error_params.cc
  pki/cert_errors.cc
  pki/cert_issuer_source_static.cc
  pki/certificate.cc
  pki/certificate_policies.cc
  pki/common_cert_errors.cc
  pki/crl.cc
  pki/encode_values.cc
  pki/extended_key_usage.cc
  pki/general_names.cc
  pki/input.cc
  pki/ip_util.cc
  pki/name_constraints.cc
  pki/ocsp.cc
  pki/ocsp_verify_result.cc
  pki/parse_certificate.cc
  pki/parse_name.cc
  pki/parse_values.cc
  pki/parsed_certificate.cc
  pki/parser.cc
  pki/path_builder.cc
  pki/pem.cc
  pki/revocation_util.cc
  pki/signature_algorithm.cc
  pki/simple_path_builder_delegate.cc
  pki/string_util.cc
  pki/trust_store.cc
  pki/trust_store_collection.cc
  pki/trust_store_in_memory.cc
  pki/verify.cc
  pki/verify_certificate_chain.cc
  pki/verify_error.cc
  pki/verify_name_match.cc
  pki/verify_signed_data.cc
)

set(
  PKI_HEADERS

  include/openssl/pki/certificate.h
  include/openssl/pki/signature_verify_cache.h
  include/openssl/pki/verify.h
  include/openssl/pki/verify_error.h
)

set(
  PKI_INTERNAL_HEADERS

  pki/cert_error_id.h
  pki/cert_error_params.h
  pki/cert_errors.h
  pki/cert_issuer_source.h
  pki/cert_issuer_source_static.h
  pki/cert_issuer_source_sync_unittest.h
  pki/certificate_policies.h
  pki/common_cert_errors.h
  pki/crl.h
  pki/encode_values.h
  pki/extended_key_usage.h
  pki/general_names.h
  pki/input.h
  pki/ip_util.h
  pki/mock_signature_verify_cache.h
  pki/name_constraints.h
  pki/nist_pkits_unittest.h
  pki/ocsp.h
  pki/ocsp_revocation_status.h
  pki/ocsp_verify_result.h
  pki/parse_certificate.h
  pki/parse_name.h
  pki/parse_values.h
  pki/parsed_certificate.h
  pki/parser.h
  pki/path_builder.h
  pki/pem.h
  pki/revocation_util.h
  pki/signature_algorithm.h
  pki/simple_path_builder_delegate.h
  pki/string_util.h
  pki/test_helpers.h
  pki/testdata/nist-pkits/pkits_testcases-inl.h
  pki/trust_store.h
  pki/trust_store_collection.h
  pki/trust_store_in_memory.h
  pki/verify_certificate_chain.h
  pki/verify_certificate_chain_typed_unittest.h
  pki/verify_name_match.h
  pki/verify_signed_data.h
)

set(
  PKI_TEST_SOURCES

  crypto/test/gtest_main.cc
  pki/cert_issuer_source_static_unittest.cc
  pki/certificate_policies_unittest.cc
  pki/certificate_unittest.cc
  pki/crl_unittest.cc
  pki/encode_values_unittest.cc
  pki/extended_key_usage_unittest.cc
  pki/general_names_unittest.cc
  pki/input_unittest.cc
  pki/ip_util_unittest.cc
  pki/mock_signature_verify_cache.cc
  pki/name_constraints_unittest.cc
  pki/nist_pkits_unittest.cc
  pki/ocsp_unittest.cc
  pki/parse_certificate_unittest.cc
  pki/parse_name_unittest.cc
  pki/parse_values_unittest.cc
  pki/parsed_certificate_unittest.cc
  pki/parser_unittest.cc
  pki/path_builder_pkits_unittest.cc
  pki/path_builder_unittest.cc
  pki/path_builder_verify_certificate_chain_unittest.cc
  pki/pem_unittest.cc
  pki/signature_algorithm_unittest.cc
  pki/simple_path_builder_delegate_unittest.cc
  pki/string_util_unittest.cc
  pki/test_helpers.cc
  pki/trust_store_collection_unittest.cc
  pki/trust_store_in_memory_unittest.cc
  pki/verify_certificate_chain_pkits_unittest.cc
  pki/verify_certificate_chain_unittest.cc
  pki/verify_name_match_unittest.cc
  pki/verify_signed_data_unittest.cc
  pki/verify_unittest.cc
)

set(
  PKI_TEST_DATA

  pki/testdata/verify_unittest/google-intermediate1.der
  pki/testdata/verify_unittest/google-intermediate2.der
  pki/testdata/verify_unittest/google-leaf.der
  pki/testdata/verify_unittest/lencr-intermediate-r3.der
  pki/testdata/verify_unittest/lencr-leaf.der
  pki/testdata/verify_unittest/lencr-root-dst-x3.der
  pki/testdata/verify_unittest/lencr-root-x1-cross-signed.der
  pki/testdata/verify_unittest/lencr-root-x1.der
  pki/testdata/verify_unittest/mozilla_roots.der
  pki/testdata/verify_unittest/self-issued.pem
)

set(
  SSL_SOURCES

  ssl/bio_ssl.cc
  ssl/d1_both.cc
  ssl/d1_lib.cc
  ssl/d1_pkt.cc
  ssl/d1_srtp.cc
  ssl/dtls_method.cc
  ssl/dtls_record.cc
  ssl/encrypted_client_hello.cc
  ssl/extensions.cc
  ssl/handoff.cc
  ssl/handshake.cc
  ssl/handshake_client.cc
  ssl/handshake_server.cc
  ssl/s3_both.cc
  ssl/s3_lib.cc
  ssl/s3_pkt.cc
  ssl/ssl_aead_ctx.cc
  ssl/ssl_asn1.cc
  ssl/ssl_buffer.cc
  ssl/ssl_cert.cc
  ssl/ssl_cipher.cc
  ssl/ssl_credential.cc
  ssl/ssl_file.cc
  ssl/ssl_key_share.cc
  ssl/ssl_lib.cc
  ssl/ssl_privkey.cc
  ssl/ssl_session.cc
  ssl/ssl_stat.cc
  ssl/ssl_transcript.cc
  ssl/ssl_versions.cc
  ssl/ssl_x509.cc
  ssl/t1_enc.cc
  ssl/tls13_both.cc
  ssl/tls13_client.cc
  ssl/tls13_enc.cc
  ssl/tls13_server.cc
  ssl/tls_method.cc
  ssl/tls_record.cc
)

set(
  SSL_HEADERS

  include/openssl/dtls1.h
  include/openssl/srtp.h
  include/openssl/ssl.h
  include/openssl/ssl3.h
  include/openssl/tls1.h
)

set(
  SSL_INTERNAL_HEADERS

  ssl/internal.h
)

set(
  SSL_TEST_SOURCES

  crypto/test/gtest_main.cc
  ssl/span_test.cc
  ssl/ssl_c_test.c
  ssl/ssl_test.cc
)

set(
  TEST_SUPPORT_SOURCES

  crypto/test/abi_test.cc
  crypto/test/file_test.cc
  crypto/test/file_test_gtest.cc
  crypto/test/file_util.cc
  crypto/test/test_data.cc
  crypto/test/test_util.cc
  crypto/test/wycheproof_util.cc
)

set(
  TEST_SUPPORT_INTERNAL_HEADERS

  crypto/test/abi_test.h
  crypto/test/file_test.h
  crypto/test/file_util.h
  crypto/test/gtest_main.h
  crypto/test/test_data.h
  crypto/test/test_util.h
  crypto/test/wycheproof_util.h
  ssl/test/async_bio.h
  ssl/test/fuzzer.h
  ssl/test/fuzzer_tags.h
  ssl/test/handshake_util.h
  ssl/test/mock_quic_transport.h
  ssl/test/packeted_bio.h
  ssl/test/settings_writer.h
  ssl/test/test_config.h
  ssl/test/test_state.h
)

set(
  TEST_SUPPORT_SOURCES_ASM

  gen/test_support/trampoline-armv4-linux.S
  gen/test_support/trampoline-armv8-apple.S
  gen/test_support/trampoline-armv8-linux.S
  gen/test_support/trampoline-armv8-win.S
  gen/test_support/trampoline-ppc-linux.S
  gen/test_support/trampoline-x86-apple.S
  gen/test_support/trampoline-x86-linux.S
  gen/test_support/trampoline-x86_64-apple.S
  gen/test_support/trampoline-x86_64-linux.S
)

set(
  TEST_SUPPORT_SOURCES_NASM

  gen/test_support/trampoline-x86-win.asm
  gen/test_support/trampoline-x86_64-win.asm
)

set(
  URANDOM_TEST_SOURCES

  crypto/fipsmodule/rand/urandom_test.cc
)
