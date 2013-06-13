#!/bin/sh
date
python BVTTest.py $1 $2 $3
date
python DefineSystemTest.py $1 $2 $3
date
python AddResourceSettingsTest.py $1 $2 $3
date
python RemoveResourceSettingsTest.py  $1 $2 $3
date
python ModifyResourceSettingTest.py  $1 $2 $3
date
python ModifySystemSettingsTest.py $1 $2 $3
date
python DestroySystemTest.py $1 $2 $3 
date
python RequestStateChangeTest.py $1 $2 $3
date
python MigrationTests.py  $1 $2 $3
date
python SnapshotTests.py $1 $2 $3
date
python StorageTests.py $1 $2 $3
date
python NetworkTests.py $1 $2 $3 
date
python MetricTests.py $1 $2 $3
date
python AssociationTest.py $1 $2 $3
date
#python PoolTests.py $1 $2 $3
