import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/foundation.dart';
import 'package:hive_flutter/hive_flutter.dart';
import 'package:path/path.dart' as p;

const _audioExtensions = {
  '.mp3',
  '.flac',
  '.wav',
  '.m4a',
  '.aac',
  '.ogg',
  '.opus',
};

/// Tracks the user's own local audio files (separate from YT Music
/// downloads). A track's Hive key is the file path itself, so re-importing
/// the same file is a no-op and "removing" never touches the original file.
class LocalLibraryService extends ChangeNotifier {
  final Box _box = Hive.box('LOCAL_MUSIC');

  LocalLibraryService() {
    _box.listenable().addListener(notifyListeners);
  }

  List<Map> get tracks => _box.values.cast<Map>().toList()
    ..sort((a, b) =>
        (a['title'] ?? '').toString().compareTo((b['title'] ?? '').toString()));

  bool _isAudioFile(String path) =>
      _audioExtensions.contains(p.extension(path).toLowerCase());

  Future<int> importFiles() async {
    final result = await FilePicker.pickFiles(
      allowMultiple: true,
      type: FileType.custom,
      allowedExtensions: ['mp3', 'flac', 'wav', 'm4a', 'aac', 'ogg', 'opus'],
    );
    if (result == null) return 0;
    int added = 0;
    for (final file in result.files) {
      if (file.path != null && await _addFile(file.path!)) added++;
    }
    return added;
  }

  Future<int> importFolder() async {
    final dirPath = await FilePicker.getDirectoryPath();
    if (dirPath == null) return 0;
    int added = 0;
    await for (final entity in Directory(dirPath).list(recursive: true)) {
      if (entity is File && _isAudioFile(entity.path)) {
        if (await _addFile(entity.path)) added++;
      }
    }
    return added;
  }

  Future<bool> _addFile(String filePath) async {
    final key = 'local::$filePath';
    if (_box.containsKey(key)) return false;
    await _box.put(key, {
      'videoId': key,
      'title': p.basenameWithoutExtension(filePath),
      'path': filePath,
      'status': 'DOWNLOADED',
      'isLocal': true,
      'thumbnails': const [],
    });
    return true;
  }

  Future<void> remove(String key) => _box.delete(key);
}
