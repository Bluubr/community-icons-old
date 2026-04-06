#pragma once

#include <string>

// Mirrors the Firestore `iconPacks` collection schema.
// Every document uses a Firebase Auto-ID and contains these fields.
struct IconPack {
    std::string id;          // Firestore document ID (Auto-ID)
    std::string name;        // Display name of the icon pack
    std::string author;      // Creator / uploader
    std::string gamemode;    // Target gamemode (e.g. "cube", "ship", "ball" …)
    std::string imageUrl;    // Firebase Storage URL for the preview thumbnail
    std::string plistUrl;    // Firebase Storage URL for the plist / icon data file
    std::string uploadedAt;  // ISO-8601 / RFC 3339 timestamp string from Firestore
    int downloads = 0;       // Download counter
};
