#ifndef AUTH_H
#define AUTH_H

#include <QString>
#include <QObject>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QRegularExpression>

#include <cstring>
#include <vector>

class QNetworkReply;

/*
Example authentication session
==============================

Client:
    * Generate one time AES key (and keep for duration of session)
    * Encrypt credentials with AES key
    * Encrypt AES key with public RSA key
    * Send encrypted AES key and credentials to server

Server:
    * Decrypt AES key using private RSA key
    * Decrypt credentials using decrypted AES key
    * Authenticate credentials
    * Sign decrypted AES key with private RSA key
    * Encrypt response with decrypted AES key
    * Send signature and response to client

Client:
    * Verify signature of signed AES key using public RSA key
    * Decrypt response using AES key
    * Proceed or otherwise, depending on response content

Details
=======

A user's password is encrypted with the public RSA key before it's then
encrypted again as part of the auth request. This is primarily so that it
can be saved as a preference and reused when the user chooses to remember
their sign in details. We can't use a conventional password hash here
because the server side is a black box whose hashing scheme we don't
necessarily know; it must be able to recover the plaintext at some point.

The auth server returns an "auth token" to the client. This contains
general permissions such as when the authorisation expires and what
features the client is able to use. The token itself takes the following
form:

    [signature][aes key][payload]

The AES key is only there to provide a level of obfuscation. As the token
is stored on the client machine as a preference or similar, encrypting it
prevents casual examination of the token without first decrypting it
using the preceeding key.

Note that the token (and indeed the server auth response) are signed
using the private key, so it is not possible to create a fake auth server
without having access to said key.

It is of course possible to binary edit the executable to skip the
authorisation procedure completely; but this is obviously an unsolvable
problem.
*/

class Auth : public QObject
{
    Q_OBJECT

public:
    struct AesKey
    {
        AesKey() = default;
        explicit AesKey(const char* bytes)
        {
            std::memcpy(_aes, &bytes[0],            sizeof(_aes));
            std::memcpy(_iv,  &bytes[sizeof(_aes)], sizeof(_iv));
        }

        unsigned char _aes[16] = {0};
        unsigned char _iv[16] = {0};
    };

    AesKey _aesKey;

    Auth();

    bool expired();
    void sendRequest(const QString& email, const QString& password);
    bool sendRequestUsingCachedCredentials();
    void reset();

    bool pluginAllowed(const QString& pluginName) const;

    bool state() const { return _authenticated; }
    QString message() const { return _message; }

    bool busy() const { return _timer.isActive(); }

private:
    QTimer _timer;
    QNetworkAccessManager _networkManager;
    QNetworkReply* _reply = nullptr;
    QString _encryptedPassword;
    bool _authenticated = false;

    QString _message;

    uint _issueTime = 0;
    uint _expiryTime = 0;
    std::vector<QRegularExpression> _allowedPluginRegexps;

    void parseAuthToken();
    void sendRequestUsingEncryptedPassword(const QString& email, const QString& encryptedPassword);

private slots:
    void onReplyReceived();
    void onTimeout();

signals:
    void stateChanged();
    void messageChanged();
    void busyChanged();
};

#endif // AUTH_H
