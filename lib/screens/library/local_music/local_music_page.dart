import 'package:flutter/material.dart';
import 'package:get_it/get_it.dart';

import '../../../services/bottom_message.dart';
import '../../../services/local_library.dart';
import '../../../services/media_player.dart';
import '../../../themes/colors.dart';
import '../../../utils/adaptive_widgets/listtile.dart';

class LocalMusicPage extends StatelessWidget {
  const LocalMusicPage({super.key});

  @override
  Widget build(BuildContext context) {
    final localLibrary = GetIt.I<LocalLibraryService>();
    return Scaffold(
      appBar: AppBar(
        title: const Text('Local Music'),
        centerTitle: true,
        actions: [
          PopupMenuButton<VoidCallback>(
            icon: const Icon(Icons.add),
            onSelected: (action) => action(),
            itemBuilder: (context) => [
              PopupMenuItem(
                value: () async {
                  final added = await localLibrary.importFiles();
                  if (context.mounted) {
                    BottomMessage.showText(context, '$added file(s) imported');
                  }
                },
                child: const Text('Import file(s)'),
              ),
              PopupMenuItem(
                value: () async {
                  final added = await localLibrary.importFolder();
                  if (context.mounted) {
                    BottomMessage.showText(context, '$added file(s) imported');
                  }
                },
                child: const Text('Import folder'),
              ),
            ],
          ),
        ],
      ),
      body: ListenableBuilder(
        listenable: localLibrary,
        builder: (context, _) {
          final tracks = localLibrary.tracks;
          if (tracks.isEmpty) {
            return const Center(
              child: Text('No local tracks yet. Import some with the + button.'),
            );
          }
          return ListView.builder(
            itemCount: tracks.length,
            itemBuilder: (context, index) {
              final track = tracks[index];
              return AdaptiveListTile(
                margin: const EdgeInsets.symmetric(vertical: 4),
                leading: Container(
                  height: 50,
                  width: 50,
                  decoration: BoxDecoration(
                    color: greyColor,
                    borderRadius: BorderRadius.circular(3),
                  ),
                  child: const Icon(Icons.music_note),
                ),
                title: Text(track['title'] ?? '', maxLines: 1),
                trailing: IconButton(
                  icon: const Icon(Icons.delete_outline),
                  onPressed: () => localLibrary.remove(track['videoId']),
                ),
                onTap: () =>
                    GetIt.I<MediaPlayer>().playAll(tracks, index: index),
              );
            },
          );
        },
      ),
    );
  }
}
