import ctypes
import os
import sys
import threading

class SingletonMeta(type):
    _instances = {}

    def __call__(cls, *args, **kwargs):
        if cls not in cls._instances:
            instance = super().__call__(*args, **kwargs)
            cls._instances[cls] = instance
        return cls._instances[cls]


class HackTV(metaclass=SingletonMeta):
    def __init__(self):
        self._thread = None
        # Charger la bibliothèque partagée
        self.hacktv = ctypes.CDLL('./hacktv.so')
        # Spécifiez les types d'arguments et de retour pour la fonction main
        # argc est un entier
        # argv est un tableau de chaînes de caractères
        # main retourne un entier
        self.hacktv.main.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_char_p)]
        self.hacktv.main.restype = ctypes.c_int
        
        # Récupérer la référence à la variable globale
        self.abort = ctypes.c_int.in_dll(self.hacktv, '_abort')

    def start(self, video_path=None):
        if self._thread is None:
            self._thread = threading.Thread(target=self.run, args=(video_path or "test:colourbars",))
            self._thread.start()
        else:
            print("Thread already running")

    def stop(self):
        if self._thread is not None:
            # Modifier la valeur de la variable globale
            self.abort.value = 1
            self._thread = None
        else:
            print("No thread to stop")

    def run(self, video_path):
      # Préparez les arguments
      print("Playing %s" % video_path)
      args = ["","-m", "l", "-f", "471250000", "-s", "16000000", "-g", "0", "--nonicam", "--repeat", "--nocolour", video_path]
      argc = len(args)
      argv = (ctypes.c_char_p * (argc + 1))()
      for i, arg in enumerate(args):
          argv[i] = ctypes.c_char_p(arg.encode('utf-8'))
      argv[argc] = None
      
      # Appelez la fonction main
      result = self.hacktv.main(argc, argv)
      self.abort.value = 0
      print("Thread stopped")

