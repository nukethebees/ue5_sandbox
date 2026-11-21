# Edit a mission in Unreal

1. Save the map. Open **Space Game Level** in the toolbar. For an existing mission, choose **Load & Apply S7** and select its entry point.
2. Place a player ship or enable **Use Observer Camera** with targets, a nonzero offset direction, and positive distance. Keep entities inside the collision grid.
3. Click **Adopt Entities**. Mark editor-owned placements **Export**, then **Export Scene**.
4. Use **Edit Mission Source** for objectives and overrides. **Validate Level**, **Set Up Playable Level**, save, and press **Play**.

Play reads the saved entry point. **Import Source** refreshes proxies. Keep each entity ID in one source. After changing a shared ship mesh or socket, use **Export Entity Geometry**. [Source ownership and examples](../../../LevelScripts/README.md)
