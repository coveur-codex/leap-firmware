"""Exercise actual Homeserver routes and export fixtures for the firmware validator.
Usage: LEAP_HOMESERVER=/path/to/leap-homeserver python tests/server_contract.py OUT
Creates only a temporary database/data directory, not the user's server data.
"""
import hashlib
from io import BytesIO
import json
import os
from pathlib import Path
import sys
import tempfile

root = Path(os.environ["LEAP_HOMESERVER"]).resolve()
os.environ["LEAP_DATABASE_URL"] = "sqlite://"
sys.path.insert(0, str(root))
os.chdir(root)  # Homeserver's built-in avatar paths are relative to its root.
from fastapi.testclient import TestClient
from sqlalchemy import create_engine, select
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool
from app.core.database import Base, get_db
from app.core.config import settings
from app.models import Device,AssetPackage,SyncEvent
from app.services import distribution
from PIL import Image
from app.services.devices import initialize_pages
from app.main import app

with tempfile.TemporaryDirectory() as temp:
    settings.data_dir = Path(temp)
    engine = create_engine("sqlite://", connect_args={"check_same_thread": False}, poolclass=StaticPool)
    Base.metadata.create_all(engine)
    session = sessionmaker(bind=engine, expire_on_commit=False)()
    device = Device(device_id="leap-test", name="Test", age=8)
    initialize_pages(device)
    session.add(device)
    session.commit()
    app.dependency_overrides[get_db] = lambda: session
    # No lifespan: route contract test must not start provider scheduler jobs.
    client = TestClient(app)
    base = "/api/v1/devices/leap-test"
    report = {"firmwareVersion": "1.0.0-beta.1", "installedAssets": {}, "freeFlash": 7000000}
    distribution.ensure_packages(session)
    package=session.get(AssetPackage,"avatar-dragon")
    old=distribution.current(session,package)
    files=distribution.file_map(old)
    png=BytesIO();Image.new("RGBA",(80,80),(60,200,180,255)).save(png,format="PNG")
    files["data/pet/idle/frame_01.png"]=distribution.store_bytes(png.getvalue())
    # Explicitly exercise a mixed legacy SVG preview + device PNG package.
    distribution.publish(session,package,files,{"preview":"preview.svg","animations":{}},package.current_version)
    session.commit()
    response = client.post(base + "/sync", json=report)
    assert response.status_code == 200, response.text
    plan = response.json()
    config = client.get(plan["configUrl"]).json()
    manifests = []
    for update in plan["assetUpdates"]:
        manifest = client.get(update["manifestUrl"]).json()
        manifests.append(manifest)
        for file in manifest["files"]:
            payload = client.get(file["url"]).content
            assert len(payload) == file["size"]
            assert hashlib.sha256(payload).hexdigest() == file["sha256"]
    url = base + "/sync/" + plan["syncId"] + "/events"
    event = {"event": "boot_success", "firmwareVersion": report["firmwareVersion"], "installedAssets": {}}
    assert client.post(url, json=event).status_code == 409  # incomplete inventory
    event["installedAssets"] = plan["desiredAssets"]
    assert client.post(url, json=event).json()["cleanupAllowed"] is True
    event["event"] = "sync_success"
    assert client.post(url, json=event).status_code == 200
    # Completed plans must be abandoned/replaced rather than retried forever.
    assert client.post(url, json=event).status_code == 409
    device.communication_enabled = False
    device.config_version += 1
    session.commit()
    new = client.post(base + "/sync", json=report).json()
    assert "communication-messages" not in new["desiredAssets"]
    assert client.get(new["configUrl"]).json()["communicationEnabled"] is False
    # beta.6 diagnostics must reach the persisted server log without changing inventory.
    target = new["assetUpdates"][0]
    failure = {"event": "download_started", "firmwareVersion": report["firmwareVersion"],
               "installedAssets": report["installedAssets"], "packageId": target["packageId"],
               "version": target["version"]}
    next_url = base + "/sync/" + new["syncId"] + "/events"
    assert client.post(next_url, json=failure).status_code == 200
    failure["event"] = "update_failed"
    failure["message"] = "Example package / example.png: Image decoder rejected file"
    before = dict(device.installed_assets)
    failed = client.post(next_url, json=failure)
    assert failed.status_code == 200 and failed.json()["cleanupAllowed"] is False
    record = session.scalar(select(SyncEvent).where(SyncEvent.run_id == new["syncId"],
                                                   SyncEvent.event == "update_failed"))
    assert record.details["packageId"] == failure["packageId"]
    assert record.details["version"] == failure["version"]
    assert record.details["message"] == failure["message"]
    session.refresh(device)
    assert device.installed_assets == before
    Path(sys.argv[1]).write_text(json.dumps({"config": config, "manifests": manifests}), encoding="utf-8")
    session.close()
print("PASS: real Homeserver sync, hashes, complete-inventory gate, closed plans, communication disable")
