# Heart rhythm follow-up

- Real detected beats should trigger the LED heart pulse immediately.
- Use the average spacing of the last five accepted beats only to fill in missed beats during gaps.
- Reject extreme timing outliers when updating that average.
- Resynchronize the predicted rhythm when a real beat arrives, avoiding a duplicate pulse if a predicted beat just fired.
- Keep the dim red glow and double-pulse appearance.

Current code loops the averaged rhythm independently of incoming detections. Revisit this behavior later; no implementation change requested on October 18, 2026.
