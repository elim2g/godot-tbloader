#include "secure_store.h"

/// Windows
#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#pragma comment(lib, "Advapi32.lib")

/// macOS
#elif defined(__APPLE__)
#include <Security/Security.h>

/// Linux
#else
#include <secret/secret.h>
#endif

void SecureStore::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("save", "key", "data"), &SecureStore::save);
    ClassDB::bind_method(D_METHOD("load", "key"), &SecureStore::load);
    ClassDB::bind_method(D_METHOD("erase", "key"), &SecureStore::erase);
}

static std::string key_to_target(const String& key)
{
    return std::string("turnt:") + key.ascii().get_data();
}

bool SecureStore::save(const String& key, const PackedByteArray &data)
{
    /// Windows
#ifdef _WIN32

    std::string target = key_to_target(key);
    CREDENTIALW cred = {0};
    cred.Type = CRED_TYPE_GENERIC;
    std::wstring wtarget(target.begin(), target.end());
    cred.TargetName = const_cast<LPWSTR>(wtarget.c_str());
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.CredentialBlobSize = static_cast<DWORD>(data.size());
    cred.CredentialBlob = const_cast<LPBYTE>(data.ptr());
    std::wstring wuser = L"turnt";
    cred.UserName = const_cast<LPWSTR>(wuser.c_str());

    return CredWriteW(&cred, 0) == TRUE;

    /// macOS
#elif defined(__APPLE__)

    CFStringRef service = CFSTR("turnt");
    CFStringRef account = CFStringCreateWithCString(NULL, key.utf8().get_data(), kCFStringEncodingUTF8);

    OSStatus status;
    {
        const void* keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
        const void* vals[] = { kSecClassGenericPassword, service, account };
        CFDictionaryRef query = CFDictionaryCreate(NULL, keys, vals, 3, NULL, NULL);
        const void* updateKeys[] = { kSecValueData };
        CFDataRef value = CFDataCreate(NULL, data.ptr(), data.size());
        const void* updateVals[] = { value };
        CFDictionaryRef upd = CFDictionaryCreate(NULL, updateKeys, updateVals, 1, NULL, NULL);
        status = SecItemUpdate(query, upd);
        
        CFRelease(value);
        CFRelease(upd);
        CFRelease(query);
    }

    if (status == errSecSuccess)
    {
        CFRelease(account);
        return true;
    }

    if (status == errSecItemNotFound)
    {
        const void* keys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData };
        CFDataRef value = CFDataCreate(NULL, data.ptr(), data.size());
        const void* vals[] = { kSecClassGenericPassword, service, account, value };
        CFDictionaryRef add = CFDictionaryCreate(NULL, keys, vals, 4, NULL, NULL);
        status = SecItemAdd(add, NULL);

        CFRelease(value);
        CFRelease(add);
        CFRelease(account);

        return status == errSecSuccess
    }

    CFRelease(account);

    return false;

    /// Linux
#else

    GError *error = nullptr;
    SecretSchema schema = {
        "org.turnt.api",
        SECRET_SCHEMA_NONE,
        { { "key", SECRET_SCHEMA_ATTRIBUTE_STRING }, { nullptr, (SecretSchemaAttributeType)0 } }
    };

    gboolean ok = secret_password_store_sync(
        &schema,
        SECRET_COLLECTION_DEFAULT,
        "TURNT API Token",
        (const gchar*)data.ptr();
        nullptr,
        &error,
        "key", 
        key.utf8().get_data(),
        nullptr
    );

    if (error)
    {
        g_error_free(error);
        return false;
    }

    return ok;

#endif
}

PackedByteArray SecureStore::load(const String& key)
{
    PackedByteArray out;

    /// Windows
#ifdef _WIN32

    auto target = key_to_target(key);
    PCREDENTIALW pcred = nullptr;
    std::wstring wtarget(target.begin(), target.end());
    if (CredReadW(wtarget.c_str(), CRED_TYPE_GENERIC, 0, &pcred)) 
    {
        out.resize(pcred->CredentialBlobSize);
        memcpy(out.ptrw(), pcred->CredentialBlob, pcred->CredentialBlobSize);
        CredFree(pcred);
    }

    return out;

    /// macOS
#elif defined(__APPLE__)

    CFStringRef service = CFSTR("turnt");
    CFStringRef account = CFStringCreateWithCString(NULL, key.utf8().get_data(), kCFStringEncodingUTF8);
    const void *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData };
    const void *vals[] = { kSecClassGenericPassword, service, account, kCFBooleanTrue };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, vals, 4, NULL, NULL);
    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result);

    CFRelease(query); 
    CFRelease(account);

    if (status == errSecSuccess && result) 
    {
        CFDataRef d = (CFDataRef)result;
        size_t n = CFDataGetLength(d);
        out.resize(n);
        memcpy(out.ptrw(), CFDataGetBytePtr(d), n);
        CFRelease(result);
    }

    return out;

    /// Linux
#else

    GError *error = nullptr;
    SecretSchema schema = 
    {
        "org.turnt.api", SECRET_SCHEMA_NONE,
        { {"key", SECRET_SCHEMA_ATTRIBUTE_STRING}, { nullptr, (SecretSchemaAttributeType)0 } }
    };
    gchar *pw = secret_password_lookup_sync(
        &schema, nullptr, &error,
        "key", key.utf8().get_data(),
        nullptr
    );

    if (error) 
    { 
        g_error_free(error); 
        return out; 
    }

    if (!pw) 
    {
        return out;
    }

    out = PackedByteArray();
    out.resize(strlen(pw));
    memcpy(out.ptrw(), pw, strlen(pw));
    secret_password_free(pw);

    return out;

#endif
}

bool SecureStore::erase(const String &key) 
{
    /// Windows
#ifdef _WIN32

    auto target = key_to_target(key);
    std::wstring wtarget(target.begin(), target.end());

    return CredDeleteW(wtarget.c_str(), CRED_TYPE_GENERIC, 0) == TRUE;

    /// macOS
#elif defined(__APPLE__)

    CFStringRef service = CFSTR("turnt");
    CFStringRef account = CFStringCreateWithCString(NULL, key.utf8().get_data(), kCFStringEncodingUTF8);
    const void *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
    const void *vals[] = { kSecClassGenericPassword, service, account };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, vals, 3, NULL, NULL);
    OSStatus status = SecItemDelete(query);

    CFRelease(query); 
    CFRelease(account);

    return (status == errSecSuccess) || (status == errSecItemNotFound);

    /// Linux
#else

    GError *error = nullptr;
    SecretSchema schema = 
    {
        "org.turnt.api", SECRET_SCHEMA_NONE,
        { {"key", SECRET_SCHEMA_ATTRIBUTE_STRING}, { nullptr, (SecretSchemaAttributeType)0 } }
    };

    gboolean ok = secret_password_clear_sync(
        &schema, nullptr, &error,
        "key", key.utf8().get_data(),
        nullptr
    );

    if (error) 
    { 
        g_error_free(error); 
        return false; 
    }

    return ok;

#endif
}