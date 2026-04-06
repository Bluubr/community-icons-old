#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <chrono>
#include <functional>
#include <vector>

using namespace geode::prelude;

// Manages Firebase Anonymous Authentication tokens for the mod.
//
// Usage:
//   FirebaseAuth::withToken([](std::string const& idToken) {
//       auto req = web::WebRequest().bodyString(body)
//                      .header("Content-Type", "application/json");
//       if (!idToken.empty())
//           req = req.header("Authorization", "Bearer " + idToken);
//       // ... spawn request ...
//   });
//
// If `firebase-api-key` is not configured the callback receives an empty
// string and the caller should proceed without an Authorization header
// (request will succeed only if Firestore rules allow unauthenticated access).
//
// Tokens are cached for 50 minutes (Firebase issues them for 60 minutes;
// the safety margin avoids using a token that is about to expire).
// Concurrent callers during a token refresh are queued and all notified once
// the new token arrives.
class FirebaseAuth {
public:
    using TokenCallback = std::function<void(std::string const& idToken)>;

    // Provides a valid Firebase idToken via `callback`.
    // May call `callback` synchronously (cache hit) or asynchronously (refresh).
    static void withToken(TokenCallback callback) {
        auto apiKey = Mod::get()->getSettingValue<std::string>("firebase-api-key");
        if (apiKey.empty()) {
            // No API key configured — proceed without auth.
            callback("");
            return;
        }

        auto now = std::chrono::steady_clock::now();
        if (!s_idToken.empty() && now < s_tokenExpiry) {
            // Cached token still valid.
            callback(s_idToken);
            return;
        }

        // Queue this caller; if a refresh is already in-flight it will notify us.
        s_pendingCallbacks.push_back(std::move(callback));
        if (s_isFetching) return;

        s_isFetching = true;
        std::string signInUrl =
            "https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=" + apiKey;

        s_authHandle = geode::async::spawn(
            web::WebRequest()
                .bodyString("{\"returnSecureToken\":true}")
                .header("Content-Type", "application/json")
                .post(signInUrl),
            [](web::WebResponse response) {
                std::string token;
                if (response.ok()) {
                    auto json = response.json();
                    if (json) {
                        auto tokenRes = (*json)["idToken"].asString();
                        if (tokenRes && !(*tokenRes).empty()) {
                            s_idToken     = *tokenRes;
                            s_tokenExpiry = std::chrono::steady_clock::now()
                                          + std::chrono::minutes(50);
                            token = s_idToken;
                        }
                    }
                }
                if (token.empty()) {
                    log::warn("FirebaseAuth: anonymous sign-in failed ({})",
                              response.code());
                }
                s_isFetching = false;
                auto callbacks = std::move(s_pendingCallbacks);
                s_pendingCallbacks.clear();
                for (auto& cb : callbacks) {
                    cb(token);
                }
            });
    }

    // Invalidates the cached token (e.g. after a 401 response).
    static void invalidate() {
        s_idToken.clear();
    }

private:
    static inline std::string                           s_idToken;
    static inline std::chrono::steady_clock::time_point s_tokenExpiry;
    static inline bool                                  s_isFetching = false;
    static inline std::vector<TokenCallback>            s_pendingCallbacks;
    static inline std::optional<arc::TaskHandle<void>>  s_authHandle;
};
