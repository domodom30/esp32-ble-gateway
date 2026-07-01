#include "security.h"
#include "stdlib.h"
#include "string.h"
#include <esp_system.h>
#include <esp_log.h>
#include <esp_types.h>
#include "Arduino.h"

Security::Security(const char *aesKey)
{
  mbedtls_aes_init(&aesContext);
  setKey(aesKey);
}

Security::Security()
{
  keyLength = 0;
  mbedtls_aes_init(&aesContext);
}

Security::~Security()
{
  mbedtls_aes_free(&aesContext);
}

void Security::generateKey(char *newKey) {
  uint8_t key[BLOCK_SIZE];
  esp_fill_random(key, BLOCK_SIZE);
  toHex(key, BLOCK_SIZE, newKey);
}

void Security::setKey(const char *aesKey)
{
  size_t hexLen = strlen(aesKey) / 2;
  uint8_t localKey[BLOCK_SIZE];
  memset(localKey, 0, BLOCK_SIZE);
  size_t copyLen = hexLen < BLOCK_SIZE ? hexLen : BLOCK_SIZE;
  fromHex(aesKey, copyLen * 2, localKey);
  if (hexLen != BLOCK_SIZE)
  {
    Serial.printf("WARNING: AES key is %u bytes, expected %u; zero-padding/truncating\n",
                   (unsigned)hexLen, (unsigned)BLOCK_SIZE);
  }
  // the key buffer is always BLOCK_SIZE (128-bit); never read past it
  keyLength = BLOCK_SIZE;
  memcpy(key, localKey, BLOCK_SIZE);
  // Program the AES decryption key schedule once here instead of on every
  // decrypt() (the key only changes via setKey). NOTE: the mbedtls context
  // holds a single key schedule; encrypt() re-programs the encryption schedule
  // for its own call, so if encrypt() is ever reintroduced (currently unused)
  // it invalidates this decrypt schedule and decrypt() must re-establish it.
  mbedtls_aes_setkey_dec(&aesContext, key, keyLength * 8);
}

void Security::getKey(uint8_t *aesKey) {
  memcpy(aesKey, key, keyLength);
}

void Security::generateIV(uint8_t IV[BLOCK_SIZE + 1])
{
  // The challenge/IV is BLOCK_SIZE (16) ASCII-hex characters: BLOCK_SIZE/2 (8)
  // random bytes expanded to 16 hex chars, plus a trailing NUL at index
  // BLOCK_SIZE. The previous loop ran for the full BLOCK_SIZE and wrote 32 hex
  // chars, overflowing the 16-byte challenge buffer by 16 bytes into the next
  // client's slot (and past the array for the last client). Only the first 16
  // bytes were ever consumed (decrypt IV + toHex(...,BLOCK_SIZE)), so producing
  // exactly 16 here is wire-compatible.
  uint8_t localIV[BLOCK_SIZE / 2];
  esp_fill_random(localIV, BLOCK_SIZE / 2);
  for (uint8_t i = 0; i < BLOCK_SIZE / 2; i++)
  {
    uint8_t nib1 = (localIV[i] >> 4) & 0x0F;
    uint8_t nib2 = (localIV[i] >> 0) & 0x0F;
    IV[i * 2 + 0] = nib1 < 0xA ? '0' + nib1 : 'A' + nib1 - 0xA;
    IV[i * 2 + 1] = nib2 < 0xA ? '0' + nib2 : 'A' + nib2 - 0xA;
  }
  IV[BLOCK_SIZE] = '\0';
}

size_t Security::getPadedSize(size_t dataLength)
{
  return dataLength + BLOCK_SIZE - dataLength % BLOCK_SIZE;
}

size_t Security::encrypt(const uint8_t IV[BLOCK_SIZE], const uint8_t *data, size_t dataLength, uint8_t *encrypted)
{
  uint8_t localIV[BLOCK_SIZE];
  memcpy(localIV, IV, BLOCK_SIZE);

  // Zero padding
  size_t padedDataLength = getPadedSize(dataLength);
  uint8_t padedData[padedDataLength];
  memset(padedData, 0, padedDataLength);
  memcpy(padedData, data, dataLength);
  mbedtls_aes_setkey_enc(&aesContext, key, keyLength * 8);
  int result = mbedtls_aes_crypt_cbc(&aesContext, MBEDTLS_AES_ENCRYPT, padedDataLength, localIV, padedData, encrypted);
  // check result
  if (result != 0)
  {
    return 0;
  }
  return padedDataLength;
}

size_t Security::decrypt(const uint8_t IV[BLOCK_SIZE], const uint8_t *data, size_t dataLength, uint8_t *decrypted)
{
  uint8_t localIV[BLOCK_SIZE];
  memcpy(localIV, IV, BLOCK_SIZE);

  // Key schedule is programmed once in setKey() (see note there).
  int result = mbedtls_aes_crypt_cbc(&aesContext, MBEDTLS_AES_DECRYPT, dataLength, localIV, data, decrypted);
  if (result != 0)
  {
    return 0;
  }
  // remove the end 0 padding ?
  return dataLength;
}

uint8_t Security::fromHex(const char *data, const size_t dataLength, uint8_t *out)
{
  uint8_t outLen = dataLength / 2;
  char tmp[3];
  tmp[2] = '\0';
  for (int i = 0; i < outLen; i++)
  {
    tmp[0] = data[i * 2];
    tmp[1] = data[i * 2 + 1];
    out[i] = strtoul(tmp, NULL, 16);
    // ESP_LOG_BUFFER_HEXDUMP("Response", tmp, 2, esp_log_level_t::ESP_LOG_INFO);
    // Serial.println(out[i], HEX);
  }
  return outLen;
}

uint8_t Security::toHex(const uint8_t *data, const size_t dataLength, char *out)
{
  for (uint8_t i = 0; i < dataLength; i++)
  {
    uint8_t nib1 = (data[i] >> 4) & 0x0F;
    uint8_t nib2 = (data[i] >> 0) & 0x0F;
    out[i * 2 + 0] = nib1 < 0xA ? '0' + nib1 : 'A' + nib1 - 0xA;
    out[i * 2 + 1] = nib2 < 0xA ? '0' + nib2 : 'A' + nib2 - 0xA;
  }
  out[dataLength * 2] = '\0';
  return dataLength * 2 + 1;
}