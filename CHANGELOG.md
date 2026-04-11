# Changelog

## [Unreleased] - Simulate Failed OTA Update

### Changed
- `version.txt` set to `v0.0.0-INVALID` to simulate a corrupted server version
- `performFirmwareUpdateOTA()` now returns `bool` (success/failure) instead of `void`
- Improved error messages with HTTP status codes and byte counts for easier debugging
- Fixed fall-through bug in `performFirmwareUpdateOTA()` where failed steps did not return early

### Added
- OTA update retry logic: device retries up to `OTA_MAX_RETRIES` (default 3) times with a configurable delay (`OTA_RETRY_DELAY_MS`, default 5s)
- Per-attempt Telegram notification so the user is informed of each retry
- Final failure Telegram alert after all retry attempts are exhausted
- `OTA_MAX_RETRIES` and `OTA_RETRY_DELAY_MS` settings in `settings.h` / `settings.cpp`

### Updated
- `update_notes.txt` with OTA failure simulation release notes
