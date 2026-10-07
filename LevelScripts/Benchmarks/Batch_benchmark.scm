;; Initial-state seed exported from Unreal Editor.

(level
  :id 'batch-benchmark
  :title "Batch Benchmark"

  :teams '(blue
    red)

  ;; Boilerplate observer camera for this playerless seed.
  :camera (camera
    :look-at '(capital-ship-blue capital-ship-red)
    :distance 331000
    :offset-direction '(-1 -1 0.7))

  :entities (list
    ;; Team: blue | Archetype: capital-ship | Count: 9
    (entity :id 'capital-ship-blue :archetype 'capital-ship :team 'blue
      :position '(168030 -20400 71162)
      :rotation '(0 90 0))
    (entity :id 'capital-ship-blue2 :archetype 'capital-ship :team 'blue
      :position '(139490 -112100 30290)
      :rotation '(0 90 0))
    (entity :id 'capital-ship-blue3 :archetype 'capital-ship :team 'blue
      :position '(168030 -29460 30290)
      :rotation '(0 90 0))
    (entity :id 'capital-ship-blue4 :archetype 'capital-ship :team 'blue
      :position '(141490 90620 30290)
      :rotation '(0 90 0))
    (entity :id 'capital-ship-blue5 :archetype 'capital-ship :team 'blue
      :position '(203760 22370 36570)
      :rotation '(0 90 0))
    (entity :id 'capital-ship-blue6 :archetype 'capital-ship :team 'blue
      :position '(168030 35111 30290)
      :rotation '(0 90 0))
    (entity :id 'capital-ship-blue7 :archetype 'capital-ship :team 'blue
      :position '(168030 76496 30290)
      :rotation '(0 90 0))
    (entity :id 'capital-ship-blue8 :archetype 'capital-ship :team 'blue
      :position '(168030 79056 65662)
      :rotation '(0 90 0))
    (entity :id 'capital-ship-blue9 :archetype 'capital-ship :team 'blue
      :position '(168030 39651 80102)
      :rotation '(0 90 0))

    ;; Team: blue | Archetype: static-turret | Count: 1442
    (entity :id 'static-turret-blue :archetype 'static-turret :team 'blue
      :position '(133570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue10 :archetype 'static-turret :team 'blue
      :position '(79570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue100 :archetype 'static-turret :team 'blue
      :position '(107570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1000 :archetype 'static-turret :team 'blue
      :position '(83570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1001 :archetype 'static-turret :team 'blue
      :position '(85570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1002 :archetype 'static-turret :team 'blue
      :position '(87570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1003 :archetype 'static-turret :team 'blue
      :position '(89570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1004 :archetype 'static-turret :team 'blue
      :position '(91570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1005 :archetype 'static-turret :team 'blue
      :position '(93570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1006 :archetype 'static-turret :team 'blue
      :position '(95570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1007 :archetype 'static-turret :team 'blue
      :position '(97570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1008 :archetype 'static-turret :team 'blue
      :position '(99570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1009 :archetype 'static-turret :team 'blue
      :position '(101570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue101 :archetype 'static-turret :team 'blue
      :position '(109570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1010 :archetype 'static-turret :team 'blue
      :position '(103570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1011 :archetype 'static-turret :team 'blue
      :position '(105570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1012 :archetype 'static-turret :team 'blue
      :position '(107570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1013 :archetype 'static-turret :team 'blue
      :position '(109570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1014 :archetype 'static-turret :team 'blue
      :position '(111570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1015 :archetype 'static-turret :team 'blue
      :position '(113570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1016 :archetype 'static-turret :team 'blue
      :position '(115570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1017 :archetype 'static-turret :team 'blue
      :position '(117570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1018 :archetype 'static-turret :team 'blue
      :position '(119570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1019 :archetype 'static-turret :team 'blue
      :position '(121570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue102 :archetype 'static-turret :team 'blue
      :position '(111570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1020 :archetype 'static-turret :team 'blue
      :position '(123570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1021 :archetype 'static-turret :team 'blue
      :position '(125570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1022 :archetype 'static-turret :team 'blue
      :position '(127570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1023 :archetype 'static-turret :team 'blue
      :position '(129570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1024 :archetype 'static-turret :team 'blue
      :position '(131570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1025 :archetype 'static-turret :team 'blue
      :position '(133570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1026 :archetype 'static-turret :team 'blue
      :position '(135570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1027 :archetype 'static-turret :team 'blue
      :position '(137570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1028 :archetype 'static-turret :team 'blue
      :position '(63570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1029 :archetype 'static-turret :team 'blue
      :position '(65570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue103 :archetype 'static-turret :team 'blue
      :position '(113570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1030 :archetype 'static-turret :team 'blue
      :position '(67570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1031 :archetype 'static-turret :team 'blue
      :position '(69570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1032 :archetype 'static-turret :team 'blue
      :position '(71570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1033 :archetype 'static-turret :team 'blue
      :position '(73570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1034 :archetype 'static-turret :team 'blue
      :position '(75570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1035 :archetype 'static-turret :team 'blue
      :position '(77570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1036 :archetype 'static-turret :team 'blue
      :position '(79570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1037 :archetype 'static-turret :team 'blue
      :position '(81570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1038 :archetype 'static-turret :team 'blue
      :position '(83570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1039 :archetype 'static-turret :team 'blue
      :position '(85570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue104 :archetype 'static-turret :team 'blue
      :position '(115570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1040 :archetype 'static-turret :team 'blue
      :position '(87570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1041 :archetype 'static-turret :team 'blue
      :position '(89570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1042 :archetype 'static-turret :team 'blue
      :position '(91570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1043 :archetype 'static-turret :team 'blue
      :position '(93570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1044 :archetype 'static-turret :team 'blue
      :position '(95570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1045 :archetype 'static-turret :team 'blue
      :position '(97570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1046 :archetype 'static-turret :team 'blue
      :position '(99570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1047 :archetype 'static-turret :team 'blue
      :position '(101570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1048 :archetype 'static-turret :team 'blue
      :position '(103570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1049 :archetype 'static-turret :team 'blue
      :position '(105570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue105 :archetype 'static-turret :team 'blue
      :position '(117570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1050 :archetype 'static-turret :team 'blue
      :position '(107570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1051 :archetype 'static-turret :team 'blue
      :position '(109570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1052 :archetype 'static-turret :team 'blue
      :position '(111570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1053 :archetype 'static-turret :team 'blue
      :position '(113570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1054 :archetype 'static-turret :team 'blue
      :position '(115570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1055 :archetype 'static-turret :team 'blue
      :position '(117570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1056 :archetype 'static-turret :team 'blue
      :position '(119570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1057 :archetype 'static-turret :team 'blue
      :position '(121570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1058 :archetype 'static-turret :team 'blue
      :position '(123570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1059 :archetype 'static-turret :team 'blue
      :position '(125570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue106 :archetype 'static-turret :team 'blue
      :position '(119570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1060 :archetype 'static-turret :team 'blue
      :position '(127570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1061 :archetype 'static-turret :team 'blue
      :position '(129570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1062 :archetype 'static-turret :team 'blue
      :position '(131570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1063 :archetype 'static-turret :team 'blue
      :position '(133570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1064 :archetype 'static-turret :team 'blue
      :position '(135570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1065 :archetype 'static-turret :team 'blue
      :position '(137570 48650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1066 :archetype 'static-turret :team 'blue
      :position '(63570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1067 :archetype 'static-turret :team 'blue
      :position '(65570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1068 :archetype 'static-turret :team 'blue
      :position '(67570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1069 :archetype 'static-turret :team 'blue
      :position '(69570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue107 :archetype 'static-turret :team 'blue
      :position '(121570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1070 :archetype 'static-turret :team 'blue
      :position '(71570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1071 :archetype 'static-turret :team 'blue
      :position '(73570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1072 :archetype 'static-turret :team 'blue
      :position '(75570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1073 :archetype 'static-turret :team 'blue
      :position '(77570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1074 :archetype 'static-turret :team 'blue
      :position '(79570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1075 :archetype 'static-turret :team 'blue
      :position '(81570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1076 :archetype 'static-turret :team 'blue
      :position '(83570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1077 :archetype 'static-turret :team 'blue
      :position '(85570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1078 :archetype 'static-turret :team 'blue
      :position '(87570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1079 :archetype 'static-turret :team 'blue
      :position '(89570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue108 :archetype 'static-turret :team 'blue
      :position '(123570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1080 :archetype 'static-turret :team 'blue
      :position '(91570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1081 :archetype 'static-turret :team 'blue
      :position '(93570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1082 :archetype 'static-turret :team 'blue
      :position '(95570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1083 :archetype 'static-turret :team 'blue
      :position '(97570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1084 :archetype 'static-turret :team 'blue
      :position '(99570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1085 :archetype 'static-turret :team 'blue
      :position '(101570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1086 :archetype 'static-turret :team 'blue
      :position '(103570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1087 :archetype 'static-turret :team 'blue
      :position '(105570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1088 :archetype 'static-turret :team 'blue
      :position '(107570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1089 :archetype 'static-turret :team 'blue
      :position '(109570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue109 :archetype 'static-turret :team 'blue
      :position '(125570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1090 :archetype 'static-turret :team 'blue
      :position '(111570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1091 :archetype 'static-turret :team 'blue
      :position '(113570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1092 :archetype 'static-turret :team 'blue
      :position '(115570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1093 :archetype 'static-turret :team 'blue
      :position '(117570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1094 :archetype 'static-turret :team 'blue
      :position '(119570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1095 :archetype 'static-turret :team 'blue
      :position '(121570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1096 :archetype 'static-turret :team 'blue
      :position '(123570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1097 :archetype 'static-turret :team 'blue
      :position '(125570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1098 :archetype 'static-turret :team 'blue
      :position '(127570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1099 :archetype 'static-turret :team 'blue
      :position '(129570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue11 :archetype 'static-turret :team 'blue
      :position '(81570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue110 :archetype 'static-turret :team 'blue
      :position '(127570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1100 :archetype 'static-turret :team 'blue
      :position '(131570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1101 :archetype 'static-turret :team 'blue
      :position '(133570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1102 :archetype 'static-turret :team 'blue
      :position '(135570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1103 :archetype 'static-turret :team 'blue
      :position '(137570 50650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1104 :archetype 'static-turret :team 'blue
      :position '(63570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1105 :archetype 'static-turret :team 'blue
      :position '(65570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1106 :archetype 'static-turret :team 'blue
      :position '(67570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1107 :archetype 'static-turret :team 'blue
      :position '(69570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1108 :archetype 'static-turret :team 'blue
      :position '(71570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1109 :archetype 'static-turret :team 'blue
      :position '(73570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue111 :archetype 'static-turret :team 'blue
      :position '(129570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1110 :archetype 'static-turret :team 'blue
      :position '(75570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1111 :archetype 'static-turret :team 'blue
      :position '(77570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1112 :archetype 'static-turret :team 'blue
      :position '(79570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1113 :archetype 'static-turret :team 'blue
      :position '(81570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1114 :archetype 'static-turret :team 'blue
      :position '(83570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1115 :archetype 'static-turret :team 'blue
      :position '(85570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1116 :archetype 'static-turret :team 'blue
      :position '(87570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1117 :archetype 'static-turret :team 'blue
      :position '(89570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1118 :archetype 'static-turret :team 'blue
      :position '(91570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1119 :archetype 'static-turret :team 'blue
      :position '(93570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue112 :archetype 'static-turret :team 'blue
      :position '(131570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1120 :archetype 'static-turret :team 'blue
      :position '(95570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1121 :archetype 'static-turret :team 'blue
      :position '(97570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1122 :archetype 'static-turret :team 'blue
      :position '(99570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1123 :archetype 'static-turret :team 'blue
      :position '(101570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1124 :archetype 'static-turret :team 'blue
      :position '(103570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1125 :archetype 'static-turret :team 'blue
      :position '(105570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1126 :archetype 'static-turret :team 'blue
      :position '(107570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1127 :archetype 'static-turret :team 'blue
      :position '(109570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1128 :archetype 'static-turret :team 'blue
      :position '(111570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1129 :archetype 'static-turret :team 'blue
      :position '(113570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue113 :archetype 'static-turret :team 'blue
      :position '(133570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1130 :archetype 'static-turret :team 'blue
      :position '(115570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1131 :archetype 'static-turret :team 'blue
      :position '(117570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1132 :archetype 'static-turret :team 'blue
      :position '(119570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1133 :archetype 'static-turret :team 'blue
      :position '(121570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1134 :archetype 'static-turret :team 'blue
      :position '(123570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1135 :archetype 'static-turret :team 'blue
      :position '(125570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1136 :archetype 'static-turret :team 'blue
      :position '(127570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1137 :archetype 'static-turret :team 'blue
      :position '(129570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1138 :archetype 'static-turret :team 'blue
      :position '(131570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1139 :archetype 'static-turret :team 'blue
      :position '(133570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue114 :archetype 'static-turret :team 'blue
      :position '(135570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1140 :archetype 'static-turret :team 'blue
      :position '(135570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1141 :archetype 'static-turret :team 'blue
      :position '(137570 52650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1142 :archetype 'static-turret :team 'blue
      :position '(63570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1143 :archetype 'static-turret :team 'blue
      :position '(65570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1144 :archetype 'static-turret :team 'blue
      :position '(67570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1145 :archetype 'static-turret :team 'blue
      :position '(69570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1146 :archetype 'static-turret :team 'blue
      :position '(71570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1147 :archetype 'static-turret :team 'blue
      :position '(73570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1148 :archetype 'static-turret :team 'blue
      :position '(75570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1149 :archetype 'static-turret :team 'blue
      :position '(77570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue115 :archetype 'static-turret :team 'blue
      :position '(137570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1150 :archetype 'static-turret :team 'blue
      :position '(79570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1151 :archetype 'static-turret :team 'blue
      :position '(81570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1152 :archetype 'static-turret :team 'blue
      :position '(83570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1153 :archetype 'static-turret :team 'blue
      :position '(85570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1154 :archetype 'static-turret :team 'blue
      :position '(87570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1155 :archetype 'static-turret :team 'blue
      :position '(89570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1156 :archetype 'static-turret :team 'blue
      :position '(91570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1157 :archetype 'static-turret :team 'blue
      :position '(93570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1158 :archetype 'static-turret :team 'blue
      :position '(95570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1159 :archetype 'static-turret :team 'blue
      :position '(97570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue116 :archetype 'static-turret :team 'blue
      :position '(63570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1160 :archetype 'static-turret :team 'blue
      :position '(99570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1161 :archetype 'static-turret :team 'blue
      :position '(101570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1162 :archetype 'static-turret :team 'blue
      :position '(103570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1163 :archetype 'static-turret :team 'blue
      :position '(105570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1164 :archetype 'static-turret :team 'blue
      :position '(107570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1165 :archetype 'static-turret :team 'blue
      :position '(109570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1166 :archetype 'static-turret :team 'blue
      :position '(111570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1167 :archetype 'static-turret :team 'blue
      :position '(113570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1168 :archetype 'static-turret :team 'blue
      :position '(115570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1169 :archetype 'static-turret :team 'blue
      :position '(117570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue117 :archetype 'static-turret :team 'blue
      :position '(65570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1170 :archetype 'static-turret :team 'blue
      :position '(119570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1171 :archetype 'static-turret :team 'blue
      :position '(121570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1172 :archetype 'static-turret :team 'blue
      :position '(123570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1173 :archetype 'static-turret :team 'blue
      :position '(125570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1174 :archetype 'static-turret :team 'blue
      :position '(127570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1175 :archetype 'static-turret :team 'blue
      :position '(129570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1176 :archetype 'static-turret :team 'blue
      :position '(131570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1177 :archetype 'static-turret :team 'blue
      :position '(133570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1178 :archetype 'static-turret :team 'blue
      :position '(135570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1179 :archetype 'static-turret :team 'blue
      :position '(137570 54650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue118 :archetype 'static-turret :team 'blue
      :position '(67570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1180 :archetype 'static-turret :team 'blue
      :position '(63570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1181 :archetype 'static-turret :team 'blue
      :position '(65570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1182 :archetype 'static-turret :team 'blue
      :position '(67570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1183 :archetype 'static-turret :team 'blue
      :position '(69570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1184 :archetype 'static-turret :team 'blue
      :position '(71570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1185 :archetype 'static-turret :team 'blue
      :position '(73570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1186 :archetype 'static-turret :team 'blue
      :position '(75570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1187 :archetype 'static-turret :team 'blue
      :position '(77570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1188 :archetype 'static-turret :team 'blue
      :position '(79570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1189 :archetype 'static-turret :team 'blue
      :position '(81570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue119 :archetype 'static-turret :team 'blue
      :position '(69570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1190 :archetype 'static-turret :team 'blue
      :position '(83570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1191 :archetype 'static-turret :team 'blue
      :position '(85570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1192 :archetype 'static-turret :team 'blue
      :position '(87570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1193 :archetype 'static-turret :team 'blue
      :position '(89570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1194 :archetype 'static-turret :team 'blue
      :position '(91570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1195 :archetype 'static-turret :team 'blue
      :position '(93570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1196 :archetype 'static-turret :team 'blue
      :position '(95570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1197 :archetype 'static-turret :team 'blue
      :position '(97570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1198 :archetype 'static-turret :team 'blue
      :position '(99570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1199 :archetype 'static-turret :team 'blue
      :position '(101570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue12 :archetype 'static-turret :team 'blue
      :position '(83570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue120 :archetype 'static-turret :team 'blue
      :position '(71570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1200 :archetype 'static-turret :team 'blue
      :position '(103570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1201 :archetype 'static-turret :team 'blue
      :position '(105570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1202 :archetype 'static-turret :team 'blue
      :position '(107570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1203 :archetype 'static-turret :team 'blue
      :position '(109570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1204 :archetype 'static-turret :team 'blue
      :position '(111570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1205 :archetype 'static-turret :team 'blue
      :position '(113570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1206 :archetype 'static-turret :team 'blue
      :position '(115570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1207 :archetype 'static-turret :team 'blue
      :position '(117570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1208 :archetype 'static-turret :team 'blue
      :position '(119570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1209 :archetype 'static-turret :team 'blue
      :position '(121570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue121 :archetype 'static-turret :team 'blue
      :position '(73570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1210 :archetype 'static-turret :team 'blue
      :position '(123570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1211 :archetype 'static-turret :team 'blue
      :position '(125570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1212 :archetype 'static-turret :team 'blue
      :position '(127570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1213 :archetype 'static-turret :team 'blue
      :position '(129570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1214 :archetype 'static-turret :team 'blue
      :position '(131570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1215 :archetype 'static-turret :team 'blue
      :position '(133570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1216 :archetype 'static-turret :team 'blue
      :position '(135570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1217 :archetype 'static-turret :team 'blue
      :position '(137570 56650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1218 :archetype 'static-turret :team 'blue
      :position '(63570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1219 :archetype 'static-turret :team 'blue
      :position '(65570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue122 :archetype 'static-turret :team 'blue
      :position '(75570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1220 :archetype 'static-turret :team 'blue
      :position '(67570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1221 :archetype 'static-turret :team 'blue
      :position '(69570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1222 :archetype 'static-turret :team 'blue
      :position '(71570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1223 :archetype 'static-turret :team 'blue
      :position '(73570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1224 :archetype 'static-turret :team 'blue
      :position '(75570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1225 :archetype 'static-turret :team 'blue
      :position '(77570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1226 :archetype 'static-turret :team 'blue
      :position '(79570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1227 :archetype 'static-turret :team 'blue
      :position '(81570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1228 :archetype 'static-turret :team 'blue
      :position '(83570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1229 :archetype 'static-turret :team 'blue
      :position '(85570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue123 :archetype 'static-turret :team 'blue
      :position '(77570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1230 :archetype 'static-turret :team 'blue
      :position '(87570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1231 :archetype 'static-turret :team 'blue
      :position '(89570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1232 :archetype 'static-turret :team 'blue
      :position '(91570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1233 :archetype 'static-turret :team 'blue
      :position '(93570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1234 :archetype 'static-turret :team 'blue
      :position '(95570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1235 :archetype 'static-turret :team 'blue
      :position '(97570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1236 :archetype 'static-turret :team 'blue
      :position '(99570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1237 :archetype 'static-turret :team 'blue
      :position '(101570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1238 :archetype 'static-turret :team 'blue
      :position '(103570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1239 :archetype 'static-turret :team 'blue
      :position '(105570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue124 :archetype 'static-turret :team 'blue
      :position '(79570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1240 :archetype 'static-turret :team 'blue
      :position '(107570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1241 :archetype 'static-turret :team 'blue
      :position '(109570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1242 :archetype 'static-turret :team 'blue
      :position '(111570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1243 :archetype 'static-turret :team 'blue
      :position '(113570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1244 :archetype 'static-turret :team 'blue
      :position '(115570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1245 :archetype 'static-turret :team 'blue
      :position '(117570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1246 :archetype 'static-turret :team 'blue
      :position '(119570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1247 :archetype 'static-turret :team 'blue
      :position '(121570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1248 :archetype 'static-turret :team 'blue
      :position '(123570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1249 :archetype 'static-turret :team 'blue
      :position '(125570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue125 :archetype 'static-turret :team 'blue
      :position '(81570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1250 :archetype 'static-turret :team 'blue
      :position '(127570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1251 :archetype 'static-turret :team 'blue
      :position '(129570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1252 :archetype 'static-turret :team 'blue
      :position '(131570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1253 :archetype 'static-turret :team 'blue
      :position '(133570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1254 :archetype 'static-turret :team 'blue
      :position '(135570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1255 :archetype 'static-turret :team 'blue
      :position '(137570 58650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1256 :archetype 'static-turret :team 'blue
      :position '(63570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1257 :archetype 'static-turret :team 'blue
      :position '(65570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1258 :archetype 'static-turret :team 'blue
      :position '(67570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1259 :archetype 'static-turret :team 'blue
      :position '(69570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue126 :archetype 'static-turret :team 'blue
      :position '(83570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1260 :archetype 'static-turret :team 'blue
      :position '(71570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1261 :archetype 'static-turret :team 'blue
      :position '(73570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1262 :archetype 'static-turret :team 'blue
      :position '(75570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1263 :archetype 'static-turret :team 'blue
      :position '(77570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1264 :archetype 'static-turret :team 'blue
      :position '(79570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1265 :archetype 'static-turret :team 'blue
      :position '(81570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1266 :archetype 'static-turret :team 'blue
      :position '(83570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1267 :archetype 'static-turret :team 'blue
      :position '(85570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1268 :archetype 'static-turret :team 'blue
      :position '(87570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1269 :archetype 'static-turret :team 'blue
      :position '(89570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue127 :archetype 'static-turret :team 'blue
      :position '(85570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1270 :archetype 'static-turret :team 'blue
      :position '(91570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1271 :archetype 'static-turret :team 'blue
      :position '(93570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1272 :archetype 'static-turret :team 'blue
      :position '(95570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1273 :archetype 'static-turret :team 'blue
      :position '(97570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1274 :archetype 'static-turret :team 'blue
      :position '(99570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1275 :archetype 'static-turret :team 'blue
      :position '(101570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1276 :archetype 'static-turret :team 'blue
      :position '(103570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1277 :archetype 'static-turret :team 'blue
      :position '(105570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1278 :archetype 'static-turret :team 'blue
      :position '(107570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1279 :archetype 'static-turret :team 'blue
      :position '(109570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue128 :archetype 'static-turret :team 'blue
      :position '(87570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1280 :archetype 'static-turret :team 'blue
      :position '(111570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1281 :archetype 'static-turret :team 'blue
      :position '(113570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1282 :archetype 'static-turret :team 'blue
      :position '(115570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1283 :archetype 'static-turret :team 'blue
      :position '(117570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1284 :archetype 'static-turret :team 'blue
      :position '(119570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1285 :archetype 'static-turret :team 'blue
      :position '(121570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1286 :archetype 'static-turret :team 'blue
      :position '(123570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1287 :archetype 'static-turret :team 'blue
      :position '(125570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1288 :archetype 'static-turret :team 'blue
      :position '(127570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1289 :archetype 'static-turret :team 'blue
      :position '(129570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue129 :archetype 'static-turret :team 'blue
      :position '(89570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1290 :archetype 'static-turret :team 'blue
      :position '(131570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1291 :archetype 'static-turret :team 'blue
      :position '(133570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1292 :archetype 'static-turret :team 'blue
      :position '(135570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1293 :archetype 'static-turret :team 'blue
      :position '(137570 60650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1294 :archetype 'static-turret :team 'blue
      :position '(63570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1295 :archetype 'static-turret :team 'blue
      :position '(65570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1296 :archetype 'static-turret :team 'blue
      :position '(67570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1297 :archetype 'static-turret :team 'blue
      :position '(69570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1298 :archetype 'static-turret :team 'blue
      :position '(71570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1299 :archetype 'static-turret :team 'blue
      :position '(73570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue13 :archetype 'static-turret :team 'blue
      :position '(85570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue130 :archetype 'static-turret :team 'blue
      :position '(91570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1300 :archetype 'static-turret :team 'blue
      :position '(75570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1301 :archetype 'static-turret :team 'blue
      :position '(77570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1302 :archetype 'static-turret :team 'blue
      :position '(79570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1303 :archetype 'static-turret :team 'blue
      :position '(81570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1304 :archetype 'static-turret :team 'blue
      :position '(83570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1305 :archetype 'static-turret :team 'blue
      :position '(85570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1306 :archetype 'static-turret :team 'blue
      :position '(87570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1307 :archetype 'static-turret :team 'blue
      :position '(89570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1308 :archetype 'static-turret :team 'blue
      :position '(91570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1309 :archetype 'static-turret :team 'blue
      :position '(93570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue131 :archetype 'static-turret :team 'blue
      :position '(93570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1310 :archetype 'static-turret :team 'blue
      :position '(95570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1311 :archetype 'static-turret :team 'blue
      :position '(97570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1312 :archetype 'static-turret :team 'blue
      :position '(99570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1313 :archetype 'static-turret :team 'blue
      :position '(101570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1314 :archetype 'static-turret :team 'blue
      :position '(103570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1315 :archetype 'static-turret :team 'blue
      :position '(105570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1316 :archetype 'static-turret :team 'blue
      :position '(107570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1317 :archetype 'static-turret :team 'blue
      :position '(109570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1318 :archetype 'static-turret :team 'blue
      :position '(111570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1319 :archetype 'static-turret :team 'blue
      :position '(113570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue132 :archetype 'static-turret :team 'blue
      :position '(95570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1320 :archetype 'static-turret :team 'blue
      :position '(115570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1321 :archetype 'static-turret :team 'blue
      :position '(117570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1322 :archetype 'static-turret :team 'blue
      :position '(119570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1323 :archetype 'static-turret :team 'blue
      :position '(121570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1324 :archetype 'static-turret :team 'blue
      :position '(123570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1325 :archetype 'static-turret :team 'blue
      :position '(125570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1326 :archetype 'static-turret :team 'blue
      :position '(127570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1327 :archetype 'static-turret :team 'blue
      :position '(129570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1328 :archetype 'static-turret :team 'blue
      :position '(131570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1329 :archetype 'static-turret :team 'blue
      :position '(133570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue133 :archetype 'static-turret :team 'blue
      :position '(97570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1330 :archetype 'static-turret :team 'blue
      :position '(135570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1331 :archetype 'static-turret :team 'blue
      :position '(137570 62650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1332 :archetype 'static-turret :team 'blue
      :position '(63570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1333 :archetype 'static-turret :team 'blue
      :position '(65570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1334 :archetype 'static-turret :team 'blue
      :position '(67570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1335 :archetype 'static-turret :team 'blue
      :position '(69570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1336 :archetype 'static-turret :team 'blue
      :position '(71570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1337 :archetype 'static-turret :team 'blue
      :position '(73570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1338 :archetype 'static-turret :team 'blue
      :position '(75570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1339 :archetype 'static-turret :team 'blue
      :position '(77570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue134 :archetype 'static-turret :team 'blue
      :position '(99570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1340 :archetype 'static-turret :team 'blue
      :position '(79570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1341 :archetype 'static-turret :team 'blue
      :position '(81570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1342 :archetype 'static-turret :team 'blue
      :position '(83570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1343 :archetype 'static-turret :team 'blue
      :position '(85570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1344 :archetype 'static-turret :team 'blue
      :position '(87570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1345 :archetype 'static-turret :team 'blue
      :position '(89570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1346 :archetype 'static-turret :team 'blue
      :position '(91570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1347 :archetype 'static-turret :team 'blue
      :position '(93570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1348 :archetype 'static-turret :team 'blue
      :position '(95570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1349 :archetype 'static-turret :team 'blue
      :position '(97570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue135 :archetype 'static-turret :team 'blue
      :position '(101570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1350 :archetype 'static-turret :team 'blue
      :position '(99570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1351 :archetype 'static-turret :team 'blue
      :position '(101570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1352 :archetype 'static-turret :team 'blue
      :position '(103570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1353 :archetype 'static-turret :team 'blue
      :position '(105570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1354 :archetype 'static-turret :team 'blue
      :position '(107570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1355 :archetype 'static-turret :team 'blue
      :position '(109570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1356 :archetype 'static-turret :team 'blue
      :position '(111570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1357 :archetype 'static-turret :team 'blue
      :position '(113570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1358 :archetype 'static-turret :team 'blue
      :position '(115570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1359 :archetype 'static-turret :team 'blue
      :position '(117570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue136 :archetype 'static-turret :team 'blue
      :position '(103570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1360 :archetype 'static-turret :team 'blue
      :position '(119570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1361 :archetype 'static-turret :team 'blue
      :position '(121570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1362 :archetype 'static-turret :team 'blue
      :position '(123570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1363 :archetype 'static-turret :team 'blue
      :position '(125570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1364 :archetype 'static-turret :team 'blue
      :position '(127570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1365 :archetype 'static-turret :team 'blue
      :position '(129570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1366 :archetype 'static-turret :team 'blue
      :position '(131570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1367 :archetype 'static-turret :team 'blue
      :position '(133570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1368 :archetype 'static-turret :team 'blue
      :position '(135570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1369 :archetype 'static-turret :team 'blue
      :position '(137570 64650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue137 :archetype 'static-turret :team 'blue
      :position '(105570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1370 :archetype 'static-turret :team 'blue
      :position '(63570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1371 :archetype 'static-turret :team 'blue
      :position '(65570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1372 :archetype 'static-turret :team 'blue
      :position '(67570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1373 :archetype 'static-turret :team 'blue
      :position '(69570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1374 :archetype 'static-turret :team 'blue
      :position '(71570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1375 :archetype 'static-turret :team 'blue
      :position '(73570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1376 :archetype 'static-turret :team 'blue
      :position '(75570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1377 :archetype 'static-turret :team 'blue
      :position '(77570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1378 :archetype 'static-turret :team 'blue
      :position '(79570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1379 :archetype 'static-turret :team 'blue
      :position '(81570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue138 :archetype 'static-turret :team 'blue
      :position '(107570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1380 :archetype 'static-turret :team 'blue
      :position '(83570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1381 :archetype 'static-turret :team 'blue
      :position '(85570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1382 :archetype 'static-turret :team 'blue
      :position '(87570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1383 :archetype 'static-turret :team 'blue
      :position '(89570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1384 :archetype 'static-turret :team 'blue
      :position '(91570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1385 :archetype 'static-turret :team 'blue
      :position '(93570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1386 :archetype 'static-turret :team 'blue
      :position '(95570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1387 :archetype 'static-turret :team 'blue
      :position '(97570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1388 :archetype 'static-turret :team 'blue
      :position '(99570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1389 :archetype 'static-turret :team 'blue
      :position '(101570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue139 :archetype 'static-turret :team 'blue
      :position '(109570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1390 :archetype 'static-turret :team 'blue
      :position '(103570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1391 :archetype 'static-turret :team 'blue
      :position '(105570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1392 :archetype 'static-turret :team 'blue
      :position '(107570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1393 :archetype 'static-turret :team 'blue
      :position '(109570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1394 :archetype 'static-turret :team 'blue
      :position '(111570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1395 :archetype 'static-turret :team 'blue
      :position '(113570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1396 :archetype 'static-turret :team 'blue
      :position '(115570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1397 :archetype 'static-turret :team 'blue
      :position '(117570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1398 :archetype 'static-turret :team 'blue
      :position '(119570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1399 :archetype 'static-turret :team 'blue
      :position '(121570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue14 :archetype 'static-turret :team 'blue
      :position '(87570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue140 :archetype 'static-turret :team 'blue
      :position '(111570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1400 :archetype 'static-turret :team 'blue
      :position '(123570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1401 :archetype 'static-turret :team 'blue
      :position '(125570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1402 :archetype 'static-turret :team 'blue
      :position '(127570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1403 :archetype 'static-turret :team 'blue
      :position '(129570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1404 :archetype 'static-turret :team 'blue
      :position '(131570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1405 :archetype 'static-turret :team 'blue
      :position '(133570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1406 :archetype 'static-turret :team 'blue
      :position '(135570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1407 :archetype 'static-turret :team 'blue
      :position '(137570 66650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1408 :archetype 'static-turret :team 'blue
      :position '(63570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1409 :archetype 'static-turret :team 'blue
      :position '(65570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue141 :archetype 'static-turret :team 'blue
      :position '(113570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1410 :archetype 'static-turret :team 'blue
      :position '(67570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1411 :archetype 'static-turret :team 'blue
      :position '(69570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1412 :archetype 'static-turret :team 'blue
      :position '(71570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1413 :archetype 'static-turret :team 'blue
      :position '(73570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1414 :archetype 'static-turret :team 'blue
      :position '(75570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1415 :archetype 'static-turret :team 'blue
      :position '(77570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1416 :archetype 'static-turret :team 'blue
      :position '(79570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1417 :archetype 'static-turret :team 'blue
      :position '(81570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1418 :archetype 'static-turret :team 'blue
      :position '(83570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1419 :archetype 'static-turret :team 'blue
      :position '(85570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue142 :archetype 'static-turret :team 'blue
      :position '(115570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1420 :archetype 'static-turret :team 'blue
      :position '(87570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1421 :archetype 'static-turret :team 'blue
      :position '(89570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1422 :archetype 'static-turret :team 'blue
      :position '(91570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1423 :archetype 'static-turret :team 'blue
      :position '(93570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1424 :archetype 'static-turret :team 'blue
      :position '(95570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1425 :archetype 'static-turret :team 'blue
      :position '(97570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1426 :archetype 'static-turret :team 'blue
      :position '(99570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1427 :archetype 'static-turret :team 'blue
      :position '(101570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1428 :archetype 'static-turret :team 'blue
      :position '(103570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1429 :archetype 'static-turret :team 'blue
      :position '(105570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue143 :archetype 'static-turret :team 'blue
      :position '(117570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1430 :archetype 'static-turret :team 'blue
      :position '(107570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1431 :archetype 'static-turret :team 'blue
      :position '(109570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1432 :archetype 'static-turret :team 'blue
      :position '(111570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1433 :archetype 'static-turret :team 'blue
      :position '(113570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1434 :archetype 'static-turret :team 'blue
      :position '(115570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1435 :archetype 'static-turret :team 'blue
      :position '(117570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1436 :archetype 'static-turret :team 'blue
      :position '(119570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1437 :archetype 'static-turret :team 'blue
      :position '(121570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1438 :archetype 'static-turret :team 'blue
      :position '(123570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1439 :archetype 'static-turret :team 'blue
      :position '(125570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue144 :archetype 'static-turret :team 'blue
      :position '(119570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1440 :archetype 'static-turret :team 'blue
      :position '(127570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1441 :archetype 'static-turret :team 'blue
      :position '(129570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue1442 :archetype 'static-turret :team 'blue
      :position '(131570 68650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue145 :archetype 'static-turret :team 'blue
      :position '(121570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue146 :archetype 'static-turret :team 'blue
      :position '(123570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue147 :archetype 'static-turret :team 'blue
      :position '(125570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue148 :archetype 'static-turret :team 'blue
      :position '(127570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue149 :archetype 'static-turret :team 'blue
      :position '(129570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue15 :archetype 'static-turret :team 'blue
      :position '(89570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue150 :archetype 'static-turret :team 'blue
      :position '(131570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue151 :archetype 'static-turret :team 'blue
      :position '(133570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue152 :archetype 'static-turret :team 'blue
      :position '(135570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue153 :archetype 'static-turret :team 'blue
      :position '(137570 650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue154 :archetype 'static-turret :team 'blue
      :position '(63570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue155 :archetype 'static-turret :team 'blue
      :position '(65570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue156 :archetype 'static-turret :team 'blue
      :position '(67570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue157 :archetype 'static-turret :team 'blue
      :position '(69570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue158 :archetype 'static-turret :team 'blue
      :position '(71570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue159 :archetype 'static-turret :team 'blue
      :position '(73570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue16 :archetype 'static-turret :team 'blue
      :position '(91570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue160 :archetype 'static-turret :team 'blue
      :position '(75570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue161 :archetype 'static-turret :team 'blue
      :position '(77570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue162 :archetype 'static-turret :team 'blue
      :position '(79570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue163 :archetype 'static-turret :team 'blue
      :position '(81570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue164 :archetype 'static-turret :team 'blue
      :position '(83570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue165 :archetype 'static-turret :team 'blue
      :position '(85570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue166 :archetype 'static-turret :team 'blue
      :position '(87570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue167 :archetype 'static-turret :team 'blue
      :position '(89570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue168 :archetype 'static-turret :team 'blue
      :position '(91570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue169 :archetype 'static-turret :team 'blue
      :position '(93570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue17 :archetype 'static-turret :team 'blue
      :position '(93570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue170 :archetype 'static-turret :team 'blue
      :position '(95570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue171 :archetype 'static-turret :team 'blue
      :position '(97570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue172 :archetype 'static-turret :team 'blue
      :position '(99570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue173 :archetype 'static-turret :team 'blue
      :position '(101570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue174 :archetype 'static-turret :team 'blue
      :position '(103570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue175 :archetype 'static-turret :team 'blue
      :position '(105570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue176 :archetype 'static-turret :team 'blue
      :position '(107570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue177 :archetype 'static-turret :team 'blue
      :position '(109570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue178 :archetype 'static-turret :team 'blue
      :position '(111570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue179 :archetype 'static-turret :team 'blue
      :position '(113570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue18 :archetype 'static-turret :team 'blue
      :position '(95570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue180 :archetype 'static-turret :team 'blue
      :position '(115570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue181 :archetype 'static-turret :team 'blue
      :position '(117570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue182 :archetype 'static-turret :team 'blue
      :position '(119570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue183 :archetype 'static-turret :team 'blue
      :position '(121570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue184 :archetype 'static-turret :team 'blue
      :position '(123570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue185 :archetype 'static-turret :team 'blue
      :position '(125570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue186 :archetype 'static-turret :team 'blue
      :position '(127570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue187 :archetype 'static-turret :team 'blue
      :position '(129570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue188 :archetype 'static-turret :team 'blue
      :position '(131570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue189 :archetype 'static-turret :team 'blue
      :position '(133570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue19 :archetype 'static-turret :team 'blue
      :position '(97570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue190 :archetype 'static-turret :team 'blue
      :position '(135570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue191 :archetype 'static-turret :team 'blue
      :position '(137570 2650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue192 :archetype 'static-turret :team 'blue
      :position '(63570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue193 :archetype 'static-turret :team 'blue
      :position '(65570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue194 :archetype 'static-turret :team 'blue
      :position '(67570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue195 :archetype 'static-turret :team 'blue
      :position '(69570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue196 :archetype 'static-turret :team 'blue
      :position '(71570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue197 :archetype 'static-turret :team 'blue
      :position '(73570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue198 :archetype 'static-turret :team 'blue
      :position '(75570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue199 :archetype 'static-turret :team 'blue
      :position '(77570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue2 :archetype 'static-turret :team 'blue
      :position '(63570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue20 :archetype 'static-turret :team 'blue
      :position '(99570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue200 :archetype 'static-turret :team 'blue
      :position '(79570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue201 :archetype 'static-turret :team 'blue
      :position '(81570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue202 :archetype 'static-turret :team 'blue
      :position '(83570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue203 :archetype 'static-turret :team 'blue
      :position '(85570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue204 :archetype 'static-turret :team 'blue
      :position '(87570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue205 :archetype 'static-turret :team 'blue
      :position '(89570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue206 :archetype 'static-turret :team 'blue
      :position '(91570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue207 :archetype 'static-turret :team 'blue
      :position '(93570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue208 :archetype 'static-turret :team 'blue
      :position '(95570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue209 :archetype 'static-turret :team 'blue
      :position '(97570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue21 :archetype 'static-turret :team 'blue
      :position '(101570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue210 :archetype 'static-turret :team 'blue
      :position '(99570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue211 :archetype 'static-turret :team 'blue
      :position '(101570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue212 :archetype 'static-turret :team 'blue
      :position '(103570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue213 :archetype 'static-turret :team 'blue
      :position '(105570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue214 :archetype 'static-turret :team 'blue
      :position '(107570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue215 :archetype 'static-turret :team 'blue
      :position '(109570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue216 :archetype 'static-turret :team 'blue
      :position '(111570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue217 :archetype 'static-turret :team 'blue
      :position '(113570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue218 :archetype 'static-turret :team 'blue
      :position '(115570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue219 :archetype 'static-turret :team 'blue
      :position '(117570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue22 :archetype 'static-turret :team 'blue
      :position '(103570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue220 :archetype 'static-turret :team 'blue
      :position '(119570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue221 :archetype 'static-turret :team 'blue
      :position '(121570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue222 :archetype 'static-turret :team 'blue
      :position '(123570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue223 :archetype 'static-turret :team 'blue
      :position '(125570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue224 :archetype 'static-turret :team 'blue
      :position '(127570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue225 :archetype 'static-turret :team 'blue
      :position '(129570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue226 :archetype 'static-turret :team 'blue
      :position '(131570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue227 :archetype 'static-turret :team 'blue
      :position '(133570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue228 :archetype 'static-turret :team 'blue
      :position '(135570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue229 :archetype 'static-turret :team 'blue
      :position '(137570 4650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue23 :archetype 'static-turret :team 'blue
      :position '(105570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue230 :archetype 'static-turret :team 'blue
      :position '(63570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue231 :archetype 'static-turret :team 'blue
      :position '(65570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue232 :archetype 'static-turret :team 'blue
      :position '(67570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue233 :archetype 'static-turret :team 'blue
      :position '(69570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue234 :archetype 'static-turret :team 'blue
      :position '(71570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue235 :archetype 'static-turret :team 'blue
      :position '(73570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue236 :archetype 'static-turret :team 'blue
      :position '(75570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue237 :archetype 'static-turret :team 'blue
      :position '(77570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue238 :archetype 'static-turret :team 'blue
      :position '(79570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue239 :archetype 'static-turret :team 'blue
      :position '(81570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue24 :archetype 'static-turret :team 'blue
      :position '(107570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue240 :archetype 'static-turret :team 'blue
      :position '(83570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue241 :archetype 'static-turret :team 'blue
      :position '(85570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue242 :archetype 'static-turret :team 'blue
      :position '(87570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue243 :archetype 'static-turret :team 'blue
      :position '(89570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue244 :archetype 'static-turret :team 'blue
      :position '(91570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue245 :archetype 'static-turret :team 'blue
      :position '(93570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue246 :archetype 'static-turret :team 'blue
      :position '(95570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue247 :archetype 'static-turret :team 'blue
      :position '(97570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue248 :archetype 'static-turret :team 'blue
      :position '(99570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue249 :archetype 'static-turret :team 'blue
      :position '(101570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue25 :archetype 'static-turret :team 'blue
      :position '(109570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue250 :archetype 'static-turret :team 'blue
      :position '(103570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue251 :archetype 'static-turret :team 'blue
      :position '(105570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue252 :archetype 'static-turret :team 'blue
      :position '(107570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue253 :archetype 'static-turret :team 'blue
      :position '(109570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue254 :archetype 'static-turret :team 'blue
      :position '(111570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue255 :archetype 'static-turret :team 'blue
      :position '(113570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue256 :archetype 'static-turret :team 'blue
      :position '(115570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue257 :archetype 'static-turret :team 'blue
      :position '(117570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue258 :archetype 'static-turret :team 'blue
      :position '(119570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue259 :archetype 'static-turret :team 'blue
      :position '(121570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue26 :archetype 'static-turret :team 'blue
      :position '(111570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue260 :archetype 'static-turret :team 'blue
      :position '(123570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue261 :archetype 'static-turret :team 'blue
      :position '(125570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue262 :archetype 'static-turret :team 'blue
      :position '(127570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue263 :archetype 'static-turret :team 'blue
      :position '(129570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue264 :archetype 'static-turret :team 'blue
      :position '(131570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue265 :archetype 'static-turret :team 'blue
      :position '(133570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue266 :archetype 'static-turret :team 'blue
      :position '(135570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue267 :archetype 'static-turret :team 'blue
      :position '(137570 6650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue268 :archetype 'static-turret :team 'blue
      :position '(63570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue269 :archetype 'static-turret :team 'blue
      :position '(65570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue27 :archetype 'static-turret :team 'blue
      :position '(113570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue270 :archetype 'static-turret :team 'blue
      :position '(67570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue271 :archetype 'static-turret :team 'blue
      :position '(69570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue272 :archetype 'static-turret :team 'blue
      :position '(71570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue273 :archetype 'static-turret :team 'blue
      :position '(73570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue274 :archetype 'static-turret :team 'blue
      :position '(75570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue275 :archetype 'static-turret :team 'blue
      :position '(77570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue276 :archetype 'static-turret :team 'blue
      :position '(79570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue277 :archetype 'static-turret :team 'blue
      :position '(81570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue278 :archetype 'static-turret :team 'blue
      :position '(83570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue279 :archetype 'static-turret :team 'blue
      :position '(85570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue28 :archetype 'static-turret :team 'blue
      :position '(115570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue280 :archetype 'static-turret :team 'blue
      :position '(87570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue281 :archetype 'static-turret :team 'blue
      :position '(89570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue282 :archetype 'static-turret :team 'blue
      :position '(91570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue283 :archetype 'static-turret :team 'blue
      :position '(93570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue284 :archetype 'static-turret :team 'blue
      :position '(95570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue285 :archetype 'static-turret :team 'blue
      :position '(97570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue286 :archetype 'static-turret :team 'blue
      :position '(99570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue287 :archetype 'static-turret :team 'blue
      :position '(101570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue288 :archetype 'static-turret :team 'blue
      :position '(103570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue289 :archetype 'static-turret :team 'blue
      :position '(105570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue29 :archetype 'static-turret :team 'blue
      :position '(117570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue290 :archetype 'static-turret :team 'blue
      :position '(107570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue291 :archetype 'static-turret :team 'blue
      :position '(109570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue292 :archetype 'static-turret :team 'blue
      :position '(111570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue293 :archetype 'static-turret :team 'blue
      :position '(113570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue294 :archetype 'static-turret :team 'blue
      :position '(115570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue295 :archetype 'static-turret :team 'blue
      :position '(117570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue296 :archetype 'static-turret :team 'blue
      :position '(119570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue297 :archetype 'static-turret :team 'blue
      :position '(121570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue298 :archetype 'static-turret :team 'blue
      :position '(123570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue299 :archetype 'static-turret :team 'blue
      :position '(125570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue3 :archetype 'static-turret :team 'blue
      :position '(65570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue30 :archetype 'static-turret :team 'blue
      :position '(119570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue300 :archetype 'static-turret :team 'blue
      :position '(127570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue301 :archetype 'static-turret :team 'blue
      :position '(129570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue302 :archetype 'static-turret :team 'blue
      :position '(131570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue303 :archetype 'static-turret :team 'blue
      :position '(133570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue304 :archetype 'static-turret :team 'blue
      :position '(135570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue305 :archetype 'static-turret :team 'blue
      :position '(137570 8650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue306 :archetype 'static-turret :team 'blue
      :position '(63570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue307 :archetype 'static-turret :team 'blue
      :position '(65570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue308 :archetype 'static-turret :team 'blue
      :position '(67570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue309 :archetype 'static-turret :team 'blue
      :position '(69570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue31 :archetype 'static-turret :team 'blue
      :position '(121570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue310 :archetype 'static-turret :team 'blue
      :position '(71570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue311 :archetype 'static-turret :team 'blue
      :position '(73570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue312 :archetype 'static-turret :team 'blue
      :position '(75570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue313 :archetype 'static-turret :team 'blue
      :position '(77570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue314 :archetype 'static-turret :team 'blue
      :position '(79570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue315 :archetype 'static-turret :team 'blue
      :position '(81570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue316 :archetype 'static-turret :team 'blue
      :position '(83570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue317 :archetype 'static-turret :team 'blue
      :position '(85570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue318 :archetype 'static-turret :team 'blue
      :position '(87570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue319 :archetype 'static-turret :team 'blue
      :position '(89570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue32 :archetype 'static-turret :team 'blue
      :position '(123570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue320 :archetype 'static-turret :team 'blue
      :position '(91570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue321 :archetype 'static-turret :team 'blue
      :position '(93570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue322 :archetype 'static-turret :team 'blue
      :position '(95570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue323 :archetype 'static-turret :team 'blue
      :position '(97570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue324 :archetype 'static-turret :team 'blue
      :position '(99570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue325 :archetype 'static-turret :team 'blue
      :position '(101570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue326 :archetype 'static-turret :team 'blue
      :position '(103570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue327 :archetype 'static-turret :team 'blue
      :position '(105570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue328 :archetype 'static-turret :team 'blue
      :position '(107570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue329 :archetype 'static-turret :team 'blue
      :position '(109570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue33 :archetype 'static-turret :team 'blue
      :position '(125570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue330 :archetype 'static-turret :team 'blue
      :position '(111570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue331 :archetype 'static-turret :team 'blue
      :position '(113570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue332 :archetype 'static-turret :team 'blue
      :position '(115570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue333 :archetype 'static-turret :team 'blue
      :position '(117570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue334 :archetype 'static-turret :team 'blue
      :position '(119570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue335 :archetype 'static-turret :team 'blue
      :position '(121570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue336 :archetype 'static-turret :team 'blue
      :position '(123570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue337 :archetype 'static-turret :team 'blue
      :position '(125570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue338 :archetype 'static-turret :team 'blue
      :position '(127570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue339 :archetype 'static-turret :team 'blue
      :position '(129570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue34 :archetype 'static-turret :team 'blue
      :position '(127570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue340 :archetype 'static-turret :team 'blue
      :position '(131570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue341 :archetype 'static-turret :team 'blue
      :position '(133570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue342 :archetype 'static-turret :team 'blue
      :position '(135570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue343 :archetype 'static-turret :team 'blue
      :position '(137570 10650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue344 :archetype 'static-turret :team 'blue
      :position '(63570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue345 :archetype 'static-turret :team 'blue
      :position '(65570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue346 :archetype 'static-turret :team 'blue
      :position '(67570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue347 :archetype 'static-turret :team 'blue
      :position '(69570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue348 :archetype 'static-turret :team 'blue
      :position '(71570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue349 :archetype 'static-turret :team 'blue
      :position '(73570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue35 :archetype 'static-turret :team 'blue
      :position '(129570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue350 :archetype 'static-turret :team 'blue
      :position '(75570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue351 :archetype 'static-turret :team 'blue
      :position '(77570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue352 :archetype 'static-turret :team 'blue
      :position '(79570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue353 :archetype 'static-turret :team 'blue
      :position '(81570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue354 :archetype 'static-turret :team 'blue
      :position '(83570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue355 :archetype 'static-turret :team 'blue
      :position '(85570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue356 :archetype 'static-turret :team 'blue
      :position '(87570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue357 :archetype 'static-turret :team 'blue
      :position '(89570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue358 :archetype 'static-turret :team 'blue
      :position '(91570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue359 :archetype 'static-turret :team 'blue
      :position '(93570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue36 :archetype 'static-turret :team 'blue
      :position '(131570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue360 :archetype 'static-turret :team 'blue
      :position '(95570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue361 :archetype 'static-turret :team 'blue
      :position '(97570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue362 :archetype 'static-turret :team 'blue
      :position '(99570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue363 :archetype 'static-turret :team 'blue
      :position '(101570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue364 :archetype 'static-turret :team 'blue
      :position '(103570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue365 :archetype 'static-turret :team 'blue
      :position '(105570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue366 :archetype 'static-turret :team 'blue
      :position '(107570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue367 :archetype 'static-turret :team 'blue
      :position '(109570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue368 :archetype 'static-turret :team 'blue
      :position '(111570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue369 :archetype 'static-turret :team 'blue
      :position '(113570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue37 :archetype 'static-turret :team 'blue
      :position '(133570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue370 :archetype 'static-turret :team 'blue
      :position '(115570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue371 :archetype 'static-turret :team 'blue
      :position '(117570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue372 :archetype 'static-turret :team 'blue
      :position '(119570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue373 :archetype 'static-turret :team 'blue
      :position '(121570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue374 :archetype 'static-turret :team 'blue
      :position '(123570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue375 :archetype 'static-turret :team 'blue
      :position '(125570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue376 :archetype 'static-turret :team 'blue
      :position '(127570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue377 :archetype 'static-turret :team 'blue
      :position '(129570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue378 :archetype 'static-turret :team 'blue
      :position '(131570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue379 :archetype 'static-turret :team 'blue
      :position '(133570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue38 :archetype 'static-turret :team 'blue
      :position '(135570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue380 :archetype 'static-turret :team 'blue
      :position '(135570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue381 :archetype 'static-turret :team 'blue
      :position '(137570 12650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue382 :archetype 'static-turret :team 'blue
      :position '(63570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue383 :archetype 'static-turret :team 'blue
      :position '(65570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue384 :archetype 'static-turret :team 'blue
      :position '(67570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue385 :archetype 'static-turret :team 'blue
      :position '(69570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue386 :archetype 'static-turret :team 'blue
      :position '(71570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue387 :archetype 'static-turret :team 'blue
      :position '(73570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue388 :archetype 'static-turret :team 'blue
      :position '(75570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue389 :archetype 'static-turret :team 'blue
      :position '(77570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue39 :archetype 'static-turret :team 'blue
      :position '(137570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue390 :archetype 'static-turret :team 'blue
      :position '(79570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue391 :archetype 'static-turret :team 'blue
      :position '(81570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue392 :archetype 'static-turret :team 'blue
      :position '(83570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue393 :archetype 'static-turret :team 'blue
      :position '(85570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue394 :archetype 'static-turret :team 'blue
      :position '(87570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue395 :archetype 'static-turret :team 'blue
      :position '(89570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue396 :archetype 'static-turret :team 'blue
      :position '(91570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue397 :archetype 'static-turret :team 'blue
      :position '(93570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue398 :archetype 'static-turret :team 'blue
      :position '(95570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue399 :archetype 'static-turret :team 'blue
      :position '(97570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue4 :archetype 'static-turret :team 'blue
      :position '(67570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue40 :archetype 'static-turret :team 'blue
      :position '(63570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue400 :archetype 'static-turret :team 'blue
      :position '(99570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue401 :archetype 'static-turret :team 'blue
      :position '(101570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue402 :archetype 'static-turret :team 'blue
      :position '(103570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue403 :archetype 'static-turret :team 'blue
      :position '(105570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue404 :archetype 'static-turret :team 'blue
      :position '(107570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue405 :archetype 'static-turret :team 'blue
      :position '(109570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue406 :archetype 'static-turret :team 'blue
      :position '(111570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue407 :archetype 'static-turret :team 'blue
      :position '(113570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue408 :archetype 'static-turret :team 'blue
      :position '(115570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue409 :archetype 'static-turret :team 'blue
      :position '(117570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue41 :archetype 'static-turret :team 'blue
      :position '(65570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue410 :archetype 'static-turret :team 'blue
      :position '(119570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue411 :archetype 'static-turret :team 'blue
      :position '(121570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue412 :archetype 'static-turret :team 'blue
      :position '(123570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue413 :archetype 'static-turret :team 'blue
      :position '(125570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue414 :archetype 'static-turret :team 'blue
      :position '(127570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue415 :archetype 'static-turret :team 'blue
      :position '(129570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue416 :archetype 'static-turret :team 'blue
      :position '(131570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue417 :archetype 'static-turret :team 'blue
      :position '(133570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue418 :archetype 'static-turret :team 'blue
      :position '(135570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue419 :archetype 'static-turret :team 'blue
      :position '(137570 14650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue42 :archetype 'static-turret :team 'blue
      :position '(67570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue420 :archetype 'static-turret :team 'blue
      :position '(63570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue421 :archetype 'static-turret :team 'blue
      :position '(65570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue422 :archetype 'static-turret :team 'blue
      :position '(67570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue423 :archetype 'static-turret :team 'blue
      :position '(69570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue424 :archetype 'static-turret :team 'blue
      :position '(71570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue425 :archetype 'static-turret :team 'blue
      :position '(73570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue426 :archetype 'static-turret :team 'blue
      :position '(75570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue427 :archetype 'static-turret :team 'blue
      :position '(77570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue428 :archetype 'static-turret :team 'blue
      :position '(79570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue429 :archetype 'static-turret :team 'blue
      :position '(81570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue43 :archetype 'static-turret :team 'blue
      :position '(69570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue430 :archetype 'static-turret :team 'blue
      :position '(83570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue431 :archetype 'static-turret :team 'blue
      :position '(85570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue432 :archetype 'static-turret :team 'blue
      :position '(87570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue433 :archetype 'static-turret :team 'blue
      :position '(89570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue434 :archetype 'static-turret :team 'blue
      :position '(91570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue435 :archetype 'static-turret :team 'blue
      :position '(93570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue436 :archetype 'static-turret :team 'blue
      :position '(95570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue437 :archetype 'static-turret :team 'blue
      :position '(97570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue438 :archetype 'static-turret :team 'blue
      :position '(99570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue439 :archetype 'static-turret :team 'blue
      :position '(101570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue44 :archetype 'static-turret :team 'blue
      :position '(71570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue440 :archetype 'static-turret :team 'blue
      :position '(103570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue441 :archetype 'static-turret :team 'blue
      :position '(105570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue442 :archetype 'static-turret :team 'blue
      :position '(107570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue443 :archetype 'static-turret :team 'blue
      :position '(109570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue444 :archetype 'static-turret :team 'blue
      :position '(111570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue445 :archetype 'static-turret :team 'blue
      :position '(113570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue446 :archetype 'static-turret :team 'blue
      :position '(115570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue447 :archetype 'static-turret :team 'blue
      :position '(117570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue448 :archetype 'static-turret :team 'blue
      :position '(119570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue449 :archetype 'static-turret :team 'blue
      :position '(121570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue45 :archetype 'static-turret :team 'blue
      :position '(73570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue450 :archetype 'static-turret :team 'blue
      :position '(123570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue451 :archetype 'static-turret :team 'blue
      :position '(125570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue452 :archetype 'static-turret :team 'blue
      :position '(127570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue453 :archetype 'static-turret :team 'blue
      :position '(129570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue454 :archetype 'static-turret :team 'blue
      :position '(131570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue455 :archetype 'static-turret :team 'blue
      :position '(133570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue456 :archetype 'static-turret :team 'blue
      :position '(135570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue457 :archetype 'static-turret :team 'blue
      :position '(137570 16650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue458 :archetype 'static-turret :team 'blue
      :position '(63570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue459 :archetype 'static-turret :team 'blue
      :position '(65570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue46 :archetype 'static-turret :team 'blue
      :position '(75570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue460 :archetype 'static-turret :team 'blue
      :position '(67570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue461 :archetype 'static-turret :team 'blue
      :position '(69570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue462 :archetype 'static-turret :team 'blue
      :position '(71570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue463 :archetype 'static-turret :team 'blue
      :position '(73570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue464 :archetype 'static-turret :team 'blue
      :position '(75570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue465 :archetype 'static-turret :team 'blue
      :position '(77570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue466 :archetype 'static-turret :team 'blue
      :position '(79570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue467 :archetype 'static-turret :team 'blue
      :position '(81570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue468 :archetype 'static-turret :team 'blue
      :position '(83570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue469 :archetype 'static-turret :team 'blue
      :position '(85570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue47 :archetype 'static-turret :team 'blue
      :position '(77570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue470 :archetype 'static-turret :team 'blue
      :position '(87570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue471 :archetype 'static-turret :team 'blue
      :position '(89570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue472 :archetype 'static-turret :team 'blue
      :position '(91570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue473 :archetype 'static-turret :team 'blue
      :position '(93570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue474 :archetype 'static-turret :team 'blue
      :position '(95570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue475 :archetype 'static-turret :team 'blue
      :position '(97570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue476 :archetype 'static-turret :team 'blue
      :position '(99570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue477 :archetype 'static-turret :team 'blue
      :position '(101570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue478 :archetype 'static-turret :team 'blue
      :position '(103570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue479 :archetype 'static-turret :team 'blue
      :position '(105570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue48 :archetype 'static-turret :team 'blue
      :position '(79570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue480 :archetype 'static-turret :team 'blue
      :position '(107570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue481 :archetype 'static-turret :team 'blue
      :position '(109570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue482 :archetype 'static-turret :team 'blue
      :position '(111570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue483 :archetype 'static-turret :team 'blue
      :position '(113570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue484 :archetype 'static-turret :team 'blue
      :position '(115570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue485 :archetype 'static-turret :team 'blue
      :position '(117570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue486 :archetype 'static-turret :team 'blue
      :position '(119570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue487 :archetype 'static-turret :team 'blue
      :position '(121570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue488 :archetype 'static-turret :team 'blue
      :position '(123570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue489 :archetype 'static-turret :team 'blue
      :position '(125570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue49 :archetype 'static-turret :team 'blue
      :position '(81570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue490 :archetype 'static-turret :team 'blue
      :position '(127570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue491 :archetype 'static-turret :team 'blue
      :position '(129570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue492 :archetype 'static-turret :team 'blue
      :position '(131570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue493 :archetype 'static-turret :team 'blue
      :position '(133570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue494 :archetype 'static-turret :team 'blue
      :position '(135570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue495 :archetype 'static-turret :team 'blue
      :position '(137570 18650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue496 :archetype 'static-turret :team 'blue
      :position '(63570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue497 :archetype 'static-turret :team 'blue
      :position '(65570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue498 :archetype 'static-turret :team 'blue
      :position '(67570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue499 :archetype 'static-turret :team 'blue
      :position '(69570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue5 :archetype 'static-turret :team 'blue
      :position '(69570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue50 :archetype 'static-turret :team 'blue
      :position '(83570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue500 :archetype 'static-turret :team 'blue
      :position '(71570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue501 :archetype 'static-turret :team 'blue
      :position '(73570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue502 :archetype 'static-turret :team 'blue
      :position '(75570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue503 :archetype 'static-turret :team 'blue
      :position '(77570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue504 :archetype 'static-turret :team 'blue
      :position '(79570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue505 :archetype 'static-turret :team 'blue
      :position '(81570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue506 :archetype 'static-turret :team 'blue
      :position '(83570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue507 :archetype 'static-turret :team 'blue
      :position '(85570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue508 :archetype 'static-turret :team 'blue
      :position '(87570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue509 :archetype 'static-turret :team 'blue
      :position '(89570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue51 :archetype 'static-turret :team 'blue
      :position '(85570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue510 :archetype 'static-turret :team 'blue
      :position '(91570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue511 :archetype 'static-turret :team 'blue
      :position '(93570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue512 :archetype 'static-turret :team 'blue
      :position '(95570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue513 :archetype 'static-turret :team 'blue
      :position '(97570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue514 :archetype 'static-turret :team 'blue
      :position '(99570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue515 :archetype 'static-turret :team 'blue
      :position '(101570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue516 :archetype 'static-turret :team 'blue
      :position '(103570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue517 :archetype 'static-turret :team 'blue
      :position '(105570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue518 :archetype 'static-turret :team 'blue
      :position '(107570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue519 :archetype 'static-turret :team 'blue
      :position '(109570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue52 :archetype 'static-turret :team 'blue
      :position '(87570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue520 :archetype 'static-turret :team 'blue
      :position '(111570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue521 :archetype 'static-turret :team 'blue
      :position '(113570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue522 :archetype 'static-turret :team 'blue
      :position '(115570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue523 :archetype 'static-turret :team 'blue
      :position '(117570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue524 :archetype 'static-turret :team 'blue
      :position '(119570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue525 :archetype 'static-turret :team 'blue
      :position '(121570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue526 :archetype 'static-turret :team 'blue
      :position '(123570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue527 :archetype 'static-turret :team 'blue
      :position '(125570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue528 :archetype 'static-turret :team 'blue
      :position '(127570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue529 :archetype 'static-turret :team 'blue
      :position '(129570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue53 :archetype 'static-turret :team 'blue
      :position '(89570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue530 :archetype 'static-turret :team 'blue
      :position '(131570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue531 :archetype 'static-turret :team 'blue
      :position '(133570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue532 :archetype 'static-turret :team 'blue
      :position '(135570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue533 :archetype 'static-turret :team 'blue
      :position '(137570 20650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue534 :archetype 'static-turret :team 'blue
      :position '(63570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue535 :archetype 'static-turret :team 'blue
      :position '(65570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue536 :archetype 'static-turret :team 'blue
      :position '(67570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue537 :archetype 'static-turret :team 'blue
      :position '(69570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue538 :archetype 'static-turret :team 'blue
      :position '(71570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue539 :archetype 'static-turret :team 'blue
      :position '(73570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue54 :archetype 'static-turret :team 'blue
      :position '(91570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue540 :archetype 'static-turret :team 'blue
      :position '(75570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue541 :archetype 'static-turret :team 'blue
      :position '(77570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue542 :archetype 'static-turret :team 'blue
      :position '(79570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue543 :archetype 'static-turret :team 'blue
      :position '(81570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue544 :archetype 'static-turret :team 'blue
      :position '(83570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue545 :archetype 'static-turret :team 'blue
      :position '(85570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue546 :archetype 'static-turret :team 'blue
      :position '(87570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue547 :archetype 'static-turret :team 'blue
      :position '(89570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue548 :archetype 'static-turret :team 'blue
      :position '(91570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue549 :archetype 'static-turret :team 'blue
      :position '(93570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue55 :archetype 'static-turret :team 'blue
      :position '(93570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue550 :archetype 'static-turret :team 'blue
      :position '(95570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue551 :archetype 'static-turret :team 'blue
      :position '(97570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue552 :archetype 'static-turret :team 'blue
      :position '(99570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue553 :archetype 'static-turret :team 'blue
      :position '(101570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue554 :archetype 'static-turret :team 'blue
      :position '(103570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue555 :archetype 'static-turret :team 'blue
      :position '(105570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue556 :archetype 'static-turret :team 'blue
      :position '(107570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue557 :archetype 'static-turret :team 'blue
      :position '(109570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue558 :archetype 'static-turret :team 'blue
      :position '(111570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue559 :archetype 'static-turret :team 'blue
      :position '(113570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue56 :archetype 'static-turret :team 'blue
      :position '(95570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue560 :archetype 'static-turret :team 'blue
      :position '(115570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue561 :archetype 'static-turret :team 'blue
      :position '(117570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue562 :archetype 'static-turret :team 'blue
      :position '(119570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue563 :archetype 'static-turret :team 'blue
      :position '(121570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue564 :archetype 'static-turret :team 'blue
      :position '(123570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue565 :archetype 'static-turret :team 'blue
      :position '(125570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue566 :archetype 'static-turret :team 'blue
      :position '(127570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue567 :archetype 'static-turret :team 'blue
      :position '(129570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue568 :archetype 'static-turret :team 'blue
      :position '(131570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue569 :archetype 'static-turret :team 'blue
      :position '(133570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue57 :archetype 'static-turret :team 'blue
      :position '(97570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue570 :archetype 'static-turret :team 'blue
      :position '(135570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue571 :archetype 'static-turret :team 'blue
      :position '(137570 22650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue572 :archetype 'static-turret :team 'blue
      :position '(63570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue573 :archetype 'static-turret :team 'blue
      :position '(65570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue574 :archetype 'static-turret :team 'blue
      :position '(67570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue575 :archetype 'static-turret :team 'blue
      :position '(69570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue576 :archetype 'static-turret :team 'blue
      :position '(71570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue577 :archetype 'static-turret :team 'blue
      :position '(73570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue578 :archetype 'static-turret :team 'blue
      :position '(75570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue579 :archetype 'static-turret :team 'blue
      :position '(77570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue58 :archetype 'static-turret :team 'blue
      :position '(99570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue580 :archetype 'static-turret :team 'blue
      :position '(79570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue581 :archetype 'static-turret :team 'blue
      :position '(81570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue582 :archetype 'static-turret :team 'blue
      :position '(83570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue583 :archetype 'static-turret :team 'blue
      :position '(85570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue584 :archetype 'static-turret :team 'blue
      :position '(87570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue585 :archetype 'static-turret :team 'blue
      :position '(89570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue586 :archetype 'static-turret :team 'blue
      :position '(91570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue587 :archetype 'static-turret :team 'blue
      :position '(93570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue588 :archetype 'static-turret :team 'blue
      :position '(95570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue589 :archetype 'static-turret :team 'blue
      :position '(97570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue59 :archetype 'static-turret :team 'blue
      :position '(101570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue590 :archetype 'static-turret :team 'blue
      :position '(99570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue591 :archetype 'static-turret :team 'blue
      :position '(101570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue592 :archetype 'static-turret :team 'blue
      :position '(103570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue593 :archetype 'static-turret :team 'blue
      :position '(105570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue594 :archetype 'static-turret :team 'blue
      :position '(107570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue595 :archetype 'static-turret :team 'blue
      :position '(109570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue596 :archetype 'static-turret :team 'blue
      :position '(111570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue597 :archetype 'static-turret :team 'blue
      :position '(113570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue598 :archetype 'static-turret :team 'blue
      :position '(115570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue599 :archetype 'static-turret :team 'blue
      :position '(117570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue6 :archetype 'static-turret :team 'blue
      :position '(71570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue60 :archetype 'static-turret :team 'blue
      :position '(103570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue600 :archetype 'static-turret :team 'blue
      :position '(119570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue601 :archetype 'static-turret :team 'blue
      :position '(121570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue602 :archetype 'static-turret :team 'blue
      :position '(123570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue603 :archetype 'static-turret :team 'blue
      :position '(125570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue604 :archetype 'static-turret :team 'blue
      :position '(127570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue605 :archetype 'static-turret :team 'blue
      :position '(129570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue606 :archetype 'static-turret :team 'blue
      :position '(131570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue607 :archetype 'static-turret :team 'blue
      :position '(133570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue608 :archetype 'static-turret :team 'blue
      :position '(135570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue609 :archetype 'static-turret :team 'blue
      :position '(137570 24650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue61 :archetype 'static-turret :team 'blue
      :position '(105570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue610 :archetype 'static-turret :team 'blue
      :position '(63570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue611 :archetype 'static-turret :team 'blue
      :position '(65570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue612 :archetype 'static-turret :team 'blue
      :position '(67570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue613 :archetype 'static-turret :team 'blue
      :position '(69570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue614 :archetype 'static-turret :team 'blue
      :position '(71570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue615 :archetype 'static-turret :team 'blue
      :position '(73570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue616 :archetype 'static-turret :team 'blue
      :position '(75570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue617 :archetype 'static-turret :team 'blue
      :position '(77570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue618 :archetype 'static-turret :team 'blue
      :position '(79570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue619 :archetype 'static-turret :team 'blue
      :position '(81570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue62 :archetype 'static-turret :team 'blue
      :position '(107570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue620 :archetype 'static-turret :team 'blue
      :position '(83570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue621 :archetype 'static-turret :team 'blue
      :position '(85570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue622 :archetype 'static-turret :team 'blue
      :position '(87570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue623 :archetype 'static-turret :team 'blue
      :position '(89570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue624 :archetype 'static-turret :team 'blue
      :position '(91570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue625 :archetype 'static-turret :team 'blue
      :position '(93570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue626 :archetype 'static-turret :team 'blue
      :position '(95570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue627 :archetype 'static-turret :team 'blue
      :position '(97570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue628 :archetype 'static-turret :team 'blue
      :position '(99570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue629 :archetype 'static-turret :team 'blue
      :position '(101570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue63 :archetype 'static-turret :team 'blue
      :position '(109570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue630 :archetype 'static-turret :team 'blue
      :position '(103570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue631 :archetype 'static-turret :team 'blue
      :position '(105570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue632 :archetype 'static-turret :team 'blue
      :position '(107570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue633 :archetype 'static-turret :team 'blue
      :position '(109570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue634 :archetype 'static-turret :team 'blue
      :position '(111570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue635 :archetype 'static-turret :team 'blue
      :position '(113570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue636 :archetype 'static-turret :team 'blue
      :position '(115570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue637 :archetype 'static-turret :team 'blue
      :position '(117570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue638 :archetype 'static-turret :team 'blue
      :position '(119570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue639 :archetype 'static-turret :team 'blue
      :position '(121570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue64 :archetype 'static-turret :team 'blue
      :position '(111570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue640 :archetype 'static-turret :team 'blue
      :position '(123570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue641 :archetype 'static-turret :team 'blue
      :position '(125570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue642 :archetype 'static-turret :team 'blue
      :position '(127570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue643 :archetype 'static-turret :team 'blue
      :position '(129570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue644 :archetype 'static-turret :team 'blue
      :position '(131570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue645 :archetype 'static-turret :team 'blue
      :position '(133570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue646 :archetype 'static-turret :team 'blue
      :position '(135570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue647 :archetype 'static-turret :team 'blue
      :position '(137570 26650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue648 :archetype 'static-turret :team 'blue
      :position '(63570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue649 :archetype 'static-turret :team 'blue
      :position '(65570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue65 :archetype 'static-turret :team 'blue
      :position '(113570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue650 :archetype 'static-turret :team 'blue
      :position '(67570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue651 :archetype 'static-turret :team 'blue
      :position '(69570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue652 :archetype 'static-turret :team 'blue
      :position '(71570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue653 :archetype 'static-turret :team 'blue
      :position '(73570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue654 :archetype 'static-turret :team 'blue
      :position '(75570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue655 :archetype 'static-turret :team 'blue
      :position '(77570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue656 :archetype 'static-turret :team 'blue
      :position '(79570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue657 :archetype 'static-turret :team 'blue
      :position '(81570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue658 :archetype 'static-turret :team 'blue
      :position '(83570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue659 :archetype 'static-turret :team 'blue
      :position '(85570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue66 :archetype 'static-turret :team 'blue
      :position '(115570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue660 :archetype 'static-turret :team 'blue
      :position '(87570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue661 :archetype 'static-turret :team 'blue
      :position '(89570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue662 :archetype 'static-turret :team 'blue
      :position '(91570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue663 :archetype 'static-turret :team 'blue
      :position '(93570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue664 :archetype 'static-turret :team 'blue
      :position '(95570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue665 :archetype 'static-turret :team 'blue
      :position '(97570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue666 :archetype 'static-turret :team 'blue
      :position '(99570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue667 :archetype 'static-turret :team 'blue
      :position '(101570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue668 :archetype 'static-turret :team 'blue
      :position '(103570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue669 :archetype 'static-turret :team 'blue
      :position '(105570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue67 :archetype 'static-turret :team 'blue
      :position '(117570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue670 :archetype 'static-turret :team 'blue
      :position '(107570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue671 :archetype 'static-turret :team 'blue
      :position '(109570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue672 :archetype 'static-turret :team 'blue
      :position '(111570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue673 :archetype 'static-turret :team 'blue
      :position '(113570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue674 :archetype 'static-turret :team 'blue
      :position '(115570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue675 :archetype 'static-turret :team 'blue
      :position '(117570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue676 :archetype 'static-turret :team 'blue
      :position '(119570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue677 :archetype 'static-turret :team 'blue
      :position '(121570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue678 :archetype 'static-turret :team 'blue
      :position '(123570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue679 :archetype 'static-turret :team 'blue
      :position '(125570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue68 :archetype 'static-turret :team 'blue
      :position '(119570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue680 :archetype 'static-turret :team 'blue
      :position '(127570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue681 :archetype 'static-turret :team 'blue
      :position '(129570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue682 :archetype 'static-turret :team 'blue
      :position '(131570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue683 :archetype 'static-turret :team 'blue
      :position '(133570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue684 :archetype 'static-turret :team 'blue
      :position '(135570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue685 :archetype 'static-turret :team 'blue
      :position '(137570 28650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue686 :archetype 'static-turret :team 'blue
      :position '(63570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue687 :archetype 'static-turret :team 'blue
      :position '(65570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue688 :archetype 'static-turret :team 'blue
      :position '(67570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue689 :archetype 'static-turret :team 'blue
      :position '(69570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue69 :archetype 'static-turret :team 'blue
      :position '(121570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue690 :archetype 'static-turret :team 'blue
      :position '(71570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue691 :archetype 'static-turret :team 'blue
      :position '(73570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue692 :archetype 'static-turret :team 'blue
      :position '(75570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue693 :archetype 'static-turret :team 'blue
      :position '(77570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue694 :archetype 'static-turret :team 'blue
      :position '(79570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue695 :archetype 'static-turret :team 'blue
      :position '(81570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue696 :archetype 'static-turret :team 'blue
      :position '(83570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue697 :archetype 'static-turret :team 'blue
      :position '(85570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue698 :archetype 'static-turret :team 'blue
      :position '(87570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue699 :archetype 'static-turret :team 'blue
      :position '(89570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue7 :archetype 'static-turret :team 'blue
      :position '(73570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue70 :archetype 'static-turret :team 'blue
      :position '(123570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue700 :archetype 'static-turret :team 'blue
      :position '(91570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue701 :archetype 'static-turret :team 'blue
      :position '(93570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue702 :archetype 'static-turret :team 'blue
      :position '(95570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue703 :archetype 'static-turret :team 'blue
      :position '(97570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue704 :archetype 'static-turret :team 'blue
      :position '(99570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue705 :archetype 'static-turret :team 'blue
      :position '(101570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue706 :archetype 'static-turret :team 'blue
      :position '(103570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue707 :archetype 'static-turret :team 'blue
      :position '(105570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue708 :archetype 'static-turret :team 'blue
      :position '(107570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue709 :archetype 'static-turret :team 'blue
      :position '(109570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue71 :archetype 'static-turret :team 'blue
      :position '(125570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue710 :archetype 'static-turret :team 'blue
      :position '(111570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue711 :archetype 'static-turret :team 'blue
      :position '(113570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue712 :archetype 'static-turret :team 'blue
      :position '(115570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue713 :archetype 'static-turret :team 'blue
      :position '(117570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue714 :archetype 'static-turret :team 'blue
      :position '(119570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue715 :archetype 'static-turret :team 'blue
      :position '(121570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue716 :archetype 'static-turret :team 'blue
      :position '(123570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue717 :archetype 'static-turret :team 'blue
      :position '(125570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue718 :archetype 'static-turret :team 'blue
      :position '(127570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue719 :archetype 'static-turret :team 'blue
      :position '(129570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue72 :archetype 'static-turret :team 'blue
      :position '(127570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue720 :archetype 'static-turret :team 'blue
      :position '(131570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue721 :archetype 'static-turret :team 'blue
      :position '(133570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue722 :archetype 'static-turret :team 'blue
      :position '(135570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue723 :archetype 'static-turret :team 'blue
      :position '(137570 30650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue724 :archetype 'static-turret :team 'blue
      :position '(63570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue725 :archetype 'static-turret :team 'blue
      :position '(65570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue726 :archetype 'static-turret :team 'blue
      :position '(67570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue727 :archetype 'static-turret :team 'blue
      :position '(69570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue728 :archetype 'static-turret :team 'blue
      :position '(71570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue729 :archetype 'static-turret :team 'blue
      :position '(73570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue73 :archetype 'static-turret :team 'blue
      :position '(129570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue730 :archetype 'static-turret :team 'blue
      :position '(75570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue731 :archetype 'static-turret :team 'blue
      :position '(77570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue732 :archetype 'static-turret :team 'blue
      :position '(79570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue733 :archetype 'static-turret :team 'blue
      :position '(81570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue734 :archetype 'static-turret :team 'blue
      :position '(83570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue735 :archetype 'static-turret :team 'blue
      :position '(85570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue736 :archetype 'static-turret :team 'blue
      :position '(87570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue737 :archetype 'static-turret :team 'blue
      :position '(89570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue738 :archetype 'static-turret :team 'blue
      :position '(91570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue739 :archetype 'static-turret :team 'blue
      :position '(93570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue74 :archetype 'static-turret :team 'blue
      :position '(131570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue740 :archetype 'static-turret :team 'blue
      :position '(95570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue741 :archetype 'static-turret :team 'blue
      :position '(97570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue742 :archetype 'static-turret :team 'blue
      :position '(99570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue743 :archetype 'static-turret :team 'blue
      :position '(101570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue744 :archetype 'static-turret :team 'blue
      :position '(103570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue745 :archetype 'static-turret :team 'blue
      :position '(105570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue746 :archetype 'static-turret :team 'blue
      :position '(107570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue747 :archetype 'static-turret :team 'blue
      :position '(109570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue748 :archetype 'static-turret :team 'blue
      :position '(111570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue749 :archetype 'static-turret :team 'blue
      :position '(113570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue75 :archetype 'static-turret :team 'blue
      :position '(133570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue750 :archetype 'static-turret :team 'blue
      :position '(115570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue751 :archetype 'static-turret :team 'blue
      :position '(117570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue752 :archetype 'static-turret :team 'blue
      :position '(119570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue753 :archetype 'static-turret :team 'blue
      :position '(121570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue754 :archetype 'static-turret :team 'blue
      :position '(123570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue755 :archetype 'static-turret :team 'blue
      :position '(125570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue756 :archetype 'static-turret :team 'blue
      :position '(127570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue757 :archetype 'static-turret :team 'blue
      :position '(129570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue758 :archetype 'static-turret :team 'blue
      :position '(131570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue759 :archetype 'static-turret :team 'blue
      :position '(133570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue76 :archetype 'static-turret :team 'blue
      :position '(135570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue760 :archetype 'static-turret :team 'blue
      :position '(135570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue761 :archetype 'static-turret :team 'blue
      :position '(137570 32650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue762 :archetype 'static-turret :team 'blue
      :position '(63570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue763 :archetype 'static-turret :team 'blue
      :position '(65570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue764 :archetype 'static-turret :team 'blue
      :position '(67570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue765 :archetype 'static-turret :team 'blue
      :position '(69570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue766 :archetype 'static-turret :team 'blue
      :position '(71570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue767 :archetype 'static-turret :team 'blue
      :position '(73570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue768 :archetype 'static-turret :team 'blue
      :position '(75570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue769 :archetype 'static-turret :team 'blue
      :position '(77570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue77 :archetype 'static-turret :team 'blue
      :position '(137570 -3350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue770 :archetype 'static-turret :team 'blue
      :position '(79570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue771 :archetype 'static-turret :team 'blue
      :position '(81570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue772 :archetype 'static-turret :team 'blue
      :position '(83570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue773 :archetype 'static-turret :team 'blue
      :position '(85570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue774 :archetype 'static-turret :team 'blue
      :position '(87570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue775 :archetype 'static-turret :team 'blue
      :position '(89570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue776 :archetype 'static-turret :team 'blue
      :position '(91570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue777 :archetype 'static-turret :team 'blue
      :position '(93570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue778 :archetype 'static-turret :team 'blue
      :position '(95570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue779 :archetype 'static-turret :team 'blue
      :position '(97570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue78 :archetype 'static-turret :team 'blue
      :position '(63570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue780 :archetype 'static-turret :team 'blue
      :position '(99570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue781 :archetype 'static-turret :team 'blue
      :position '(101570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue782 :archetype 'static-turret :team 'blue
      :position '(103570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue783 :archetype 'static-turret :team 'blue
      :position '(105570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue784 :archetype 'static-turret :team 'blue
      :position '(107570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue785 :archetype 'static-turret :team 'blue
      :position '(109570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue786 :archetype 'static-turret :team 'blue
      :position '(111570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue787 :archetype 'static-turret :team 'blue
      :position '(113570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue788 :archetype 'static-turret :team 'blue
      :position '(115570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue789 :archetype 'static-turret :team 'blue
      :position '(117570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue79 :archetype 'static-turret :team 'blue
      :position '(65570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue790 :archetype 'static-turret :team 'blue
      :position '(119570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue791 :archetype 'static-turret :team 'blue
      :position '(121570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue792 :archetype 'static-turret :team 'blue
      :position '(123570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue793 :archetype 'static-turret :team 'blue
      :position '(125570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue794 :archetype 'static-turret :team 'blue
      :position '(127570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue795 :archetype 'static-turret :team 'blue
      :position '(129570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue796 :archetype 'static-turret :team 'blue
      :position '(131570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue797 :archetype 'static-turret :team 'blue
      :position '(133570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue798 :archetype 'static-turret :team 'blue
      :position '(135570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue799 :archetype 'static-turret :team 'blue
      :position '(137570 34650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue8 :archetype 'static-turret :team 'blue
      :position '(75570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue80 :archetype 'static-turret :team 'blue
      :position '(67570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue800 :archetype 'static-turret :team 'blue
      :position '(63570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue801 :archetype 'static-turret :team 'blue
      :position '(65570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue802 :archetype 'static-turret :team 'blue
      :position '(67570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue803 :archetype 'static-turret :team 'blue
      :position '(69570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue804 :archetype 'static-turret :team 'blue
      :position '(71570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue805 :archetype 'static-turret :team 'blue
      :position '(73570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue806 :archetype 'static-turret :team 'blue
      :position '(75570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue807 :archetype 'static-turret :team 'blue
      :position '(77570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue808 :archetype 'static-turret :team 'blue
      :position '(79570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue809 :archetype 'static-turret :team 'blue
      :position '(81570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue81 :archetype 'static-turret :team 'blue
      :position '(69570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue810 :archetype 'static-turret :team 'blue
      :position '(83570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue811 :archetype 'static-turret :team 'blue
      :position '(85570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue812 :archetype 'static-turret :team 'blue
      :position '(87570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue813 :archetype 'static-turret :team 'blue
      :position '(89570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue814 :archetype 'static-turret :team 'blue
      :position '(91570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue815 :archetype 'static-turret :team 'blue
      :position '(93570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue816 :archetype 'static-turret :team 'blue
      :position '(95570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue817 :archetype 'static-turret :team 'blue
      :position '(97570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue818 :archetype 'static-turret :team 'blue
      :position '(99570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue819 :archetype 'static-turret :team 'blue
      :position '(101570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue82 :archetype 'static-turret :team 'blue
      :position '(71570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue820 :archetype 'static-turret :team 'blue
      :position '(103570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue821 :archetype 'static-turret :team 'blue
      :position '(105570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue822 :archetype 'static-turret :team 'blue
      :position '(107570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue823 :archetype 'static-turret :team 'blue
      :position '(109570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue824 :archetype 'static-turret :team 'blue
      :position '(111570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue825 :archetype 'static-turret :team 'blue
      :position '(113570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue826 :archetype 'static-turret :team 'blue
      :position '(115570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue827 :archetype 'static-turret :team 'blue
      :position '(117570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue828 :archetype 'static-turret :team 'blue
      :position '(119570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue829 :archetype 'static-turret :team 'blue
      :position '(121570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue83 :archetype 'static-turret :team 'blue
      :position '(73570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue830 :archetype 'static-turret :team 'blue
      :position '(123570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue831 :archetype 'static-turret :team 'blue
      :position '(125570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue832 :archetype 'static-turret :team 'blue
      :position '(127570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue833 :archetype 'static-turret :team 'blue
      :position '(129570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue834 :archetype 'static-turret :team 'blue
      :position '(131570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue835 :archetype 'static-turret :team 'blue
      :position '(133570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue836 :archetype 'static-turret :team 'blue
      :position '(135570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue837 :archetype 'static-turret :team 'blue
      :position '(137570 36650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue838 :archetype 'static-turret :team 'blue
      :position '(63570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue839 :archetype 'static-turret :team 'blue
      :position '(65570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue84 :archetype 'static-turret :team 'blue
      :position '(75570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue840 :archetype 'static-turret :team 'blue
      :position '(67570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue841 :archetype 'static-turret :team 'blue
      :position '(69570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue842 :archetype 'static-turret :team 'blue
      :position '(71570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue843 :archetype 'static-turret :team 'blue
      :position '(73570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue844 :archetype 'static-turret :team 'blue
      :position '(75570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue845 :archetype 'static-turret :team 'blue
      :position '(77570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue846 :archetype 'static-turret :team 'blue
      :position '(79570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue847 :archetype 'static-turret :team 'blue
      :position '(81570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue848 :archetype 'static-turret :team 'blue
      :position '(83570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue849 :archetype 'static-turret :team 'blue
      :position '(85570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue85 :archetype 'static-turret :team 'blue
      :position '(77570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue850 :archetype 'static-turret :team 'blue
      :position '(87570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue851 :archetype 'static-turret :team 'blue
      :position '(89570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue852 :archetype 'static-turret :team 'blue
      :position '(91570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue853 :archetype 'static-turret :team 'blue
      :position '(93570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue854 :archetype 'static-turret :team 'blue
      :position '(95570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue855 :archetype 'static-turret :team 'blue
      :position '(97570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue856 :archetype 'static-turret :team 'blue
      :position '(99570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue857 :archetype 'static-turret :team 'blue
      :position '(101570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue858 :archetype 'static-turret :team 'blue
      :position '(103570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue859 :archetype 'static-turret :team 'blue
      :position '(105570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue86 :archetype 'static-turret :team 'blue
      :position '(79570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue860 :archetype 'static-turret :team 'blue
      :position '(107570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue861 :archetype 'static-turret :team 'blue
      :position '(109570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue862 :archetype 'static-turret :team 'blue
      :position '(111570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue863 :archetype 'static-turret :team 'blue
      :position '(113570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue864 :archetype 'static-turret :team 'blue
      :position '(115570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue865 :archetype 'static-turret :team 'blue
      :position '(117570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue866 :archetype 'static-turret :team 'blue
      :position '(119570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue867 :archetype 'static-turret :team 'blue
      :position '(121570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue868 :archetype 'static-turret :team 'blue
      :position '(123570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue869 :archetype 'static-turret :team 'blue
      :position '(125570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue87 :archetype 'static-turret :team 'blue
      :position '(81570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue870 :archetype 'static-turret :team 'blue
      :position '(127570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue871 :archetype 'static-turret :team 'blue
      :position '(129570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue872 :archetype 'static-turret :team 'blue
      :position '(131570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue873 :archetype 'static-turret :team 'blue
      :position '(133570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue874 :archetype 'static-turret :team 'blue
      :position '(135570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue875 :archetype 'static-turret :team 'blue
      :position '(137570 38650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue876 :archetype 'static-turret :team 'blue
      :position '(63570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue877 :archetype 'static-turret :team 'blue
      :position '(65570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue878 :archetype 'static-turret :team 'blue
      :position '(67570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue879 :archetype 'static-turret :team 'blue
      :position '(69570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue88 :archetype 'static-turret :team 'blue
      :position '(83570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue880 :archetype 'static-turret :team 'blue
      :position '(71570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue881 :archetype 'static-turret :team 'blue
      :position '(73570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue882 :archetype 'static-turret :team 'blue
      :position '(75570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue883 :archetype 'static-turret :team 'blue
      :position '(77570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue884 :archetype 'static-turret :team 'blue
      :position '(79570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue885 :archetype 'static-turret :team 'blue
      :position '(81570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue886 :archetype 'static-turret :team 'blue
      :position '(83570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue887 :archetype 'static-turret :team 'blue
      :position '(85570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue888 :archetype 'static-turret :team 'blue
      :position '(87570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue889 :archetype 'static-turret :team 'blue
      :position '(89570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue89 :archetype 'static-turret :team 'blue
      :position '(85570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue890 :archetype 'static-turret :team 'blue
      :position '(91570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue891 :archetype 'static-turret :team 'blue
      :position '(93570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue892 :archetype 'static-turret :team 'blue
      :position '(95570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue893 :archetype 'static-turret :team 'blue
      :position '(97570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue894 :archetype 'static-turret :team 'blue
      :position '(99570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue895 :archetype 'static-turret :team 'blue
      :position '(101570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue896 :archetype 'static-turret :team 'blue
      :position '(103570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue897 :archetype 'static-turret :team 'blue
      :position '(105570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue898 :archetype 'static-turret :team 'blue
      :position '(107570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue899 :archetype 'static-turret :team 'blue
      :position '(109570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue9 :archetype 'static-turret :team 'blue
      :position '(77570 -5350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue90 :archetype 'static-turret :team 'blue
      :position '(87570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue900 :archetype 'static-turret :team 'blue
      :position '(111570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue901 :archetype 'static-turret :team 'blue
      :position '(113570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue902 :archetype 'static-turret :team 'blue
      :position '(115570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue903 :archetype 'static-turret :team 'blue
      :position '(117570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue904 :archetype 'static-turret :team 'blue
      :position '(119570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue905 :archetype 'static-turret :team 'blue
      :position '(121570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue906 :archetype 'static-turret :team 'blue
      :position '(123570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue907 :archetype 'static-turret :team 'blue
      :position '(125570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue908 :archetype 'static-turret :team 'blue
      :position '(127570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue909 :archetype 'static-turret :team 'blue
      :position '(129570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue91 :archetype 'static-turret :team 'blue
      :position '(89570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue910 :archetype 'static-turret :team 'blue
      :position '(131570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue911 :archetype 'static-turret :team 'blue
      :position '(133570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue912 :archetype 'static-turret :team 'blue
      :position '(135570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue913 :archetype 'static-turret :team 'blue
      :position '(137570 40650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue914 :archetype 'static-turret :team 'blue
      :position '(63570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue915 :archetype 'static-turret :team 'blue
      :position '(65570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue916 :archetype 'static-turret :team 'blue
      :position '(67570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue917 :archetype 'static-turret :team 'blue
      :position '(69570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue918 :archetype 'static-turret :team 'blue
      :position '(71570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue919 :archetype 'static-turret :team 'blue
      :position '(73570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue92 :archetype 'static-turret :team 'blue
      :position '(91570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue920 :archetype 'static-turret :team 'blue
      :position '(75570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue921 :archetype 'static-turret :team 'blue
      :position '(77570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue922 :archetype 'static-turret :team 'blue
      :position '(79570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue923 :archetype 'static-turret :team 'blue
      :position '(81570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue924 :archetype 'static-turret :team 'blue
      :position '(83570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue925 :archetype 'static-turret :team 'blue
      :position '(85570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue926 :archetype 'static-turret :team 'blue
      :position '(87570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue927 :archetype 'static-turret :team 'blue
      :position '(89570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue928 :archetype 'static-turret :team 'blue
      :position '(91570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue929 :archetype 'static-turret :team 'blue
      :position '(93570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue93 :archetype 'static-turret :team 'blue
      :position '(93570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue930 :archetype 'static-turret :team 'blue
      :position '(95570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue931 :archetype 'static-turret :team 'blue
      :position '(97570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue932 :archetype 'static-turret :team 'blue
      :position '(99570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue933 :archetype 'static-turret :team 'blue
      :position '(101570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue934 :archetype 'static-turret :team 'blue
      :position '(103570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue935 :archetype 'static-turret :team 'blue
      :position '(105570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue936 :archetype 'static-turret :team 'blue
      :position '(107570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue937 :archetype 'static-turret :team 'blue
      :position '(109570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue938 :archetype 'static-turret :team 'blue
      :position '(111570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue939 :archetype 'static-turret :team 'blue
      :position '(113570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue94 :archetype 'static-turret :team 'blue
      :position '(95570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue940 :archetype 'static-turret :team 'blue
      :position '(115570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue941 :archetype 'static-turret :team 'blue
      :position '(117570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue942 :archetype 'static-turret :team 'blue
      :position '(119570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue943 :archetype 'static-turret :team 'blue
      :position '(121570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue944 :archetype 'static-turret :team 'blue
      :position '(123570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue945 :archetype 'static-turret :team 'blue
      :position '(125570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue946 :archetype 'static-turret :team 'blue
      :position '(127570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue947 :archetype 'static-turret :team 'blue
      :position '(129570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue948 :archetype 'static-turret :team 'blue
      :position '(131570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue949 :archetype 'static-turret :team 'blue
      :position '(133570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue95 :archetype 'static-turret :team 'blue
      :position '(97570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue950 :archetype 'static-turret :team 'blue
      :position '(135570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue951 :archetype 'static-turret :team 'blue
      :position '(137570 42650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue952 :archetype 'static-turret :team 'blue
      :position '(63570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue953 :archetype 'static-turret :team 'blue
      :position '(65570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue954 :archetype 'static-turret :team 'blue
      :position '(67570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue955 :archetype 'static-turret :team 'blue
      :position '(69570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue956 :archetype 'static-turret :team 'blue
      :position '(71570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue957 :archetype 'static-turret :team 'blue
      :position '(73570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue958 :archetype 'static-turret :team 'blue
      :position '(75570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue959 :archetype 'static-turret :team 'blue
      :position '(77570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue96 :archetype 'static-turret :team 'blue
      :position '(99570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue960 :archetype 'static-turret :team 'blue
      :position '(79570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue961 :archetype 'static-turret :team 'blue
      :position '(81570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue962 :archetype 'static-turret :team 'blue
      :position '(83570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue963 :archetype 'static-turret :team 'blue
      :position '(85570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue964 :archetype 'static-turret :team 'blue
      :position '(87570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue965 :archetype 'static-turret :team 'blue
      :position '(89570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue966 :archetype 'static-turret :team 'blue
      :position '(91570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue967 :archetype 'static-turret :team 'blue
      :position '(93570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue968 :archetype 'static-turret :team 'blue
      :position '(95570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue969 :archetype 'static-turret :team 'blue
      :position '(97570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue97 :archetype 'static-turret :team 'blue
      :position '(101570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue970 :archetype 'static-turret :team 'blue
      :position '(99570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue971 :archetype 'static-turret :team 'blue
      :position '(101570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue972 :archetype 'static-turret :team 'blue
      :position '(103570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue973 :archetype 'static-turret :team 'blue
      :position '(105570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue974 :archetype 'static-turret :team 'blue
      :position '(107570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue975 :archetype 'static-turret :team 'blue
      :position '(109570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue976 :archetype 'static-turret :team 'blue
      :position '(111570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue977 :archetype 'static-turret :team 'blue
      :position '(113570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue978 :archetype 'static-turret :team 'blue
      :position '(115570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue979 :archetype 'static-turret :team 'blue
      :position '(117570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue98 :archetype 'static-turret :team 'blue
      :position '(103570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue980 :archetype 'static-turret :team 'blue
      :position '(119570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue981 :archetype 'static-turret :team 'blue
      :position '(121570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue982 :archetype 'static-turret :team 'blue
      :position '(123570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue983 :archetype 'static-turret :team 'blue
      :position '(125570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue984 :archetype 'static-turret :team 'blue
      :position '(127570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue985 :archetype 'static-turret :team 'blue
      :position '(129570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue986 :archetype 'static-turret :team 'blue
      :position '(131570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue987 :archetype 'static-turret :team 'blue
      :position '(133570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue988 :archetype 'static-turret :team 'blue
      :position '(135570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue989 :archetype 'static-turret :team 'blue
      :position '(137570 44650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue99 :archetype 'static-turret :team 'blue
      :position '(105570 -1350 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue990 :archetype 'static-turret :team 'blue
      :position '(63570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue991 :archetype 'static-turret :team 'blue
      :position '(65570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue992 :archetype 'static-turret :team 'blue
      :position '(67570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue993 :archetype 'static-turret :team 'blue
      :position '(69570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue994 :archetype 'static-turret :team 'blue
      :position '(71570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue995 :archetype 'static-turret :team 'blue
      :position '(73570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue996 :archetype 'static-turret :team 'blue
      :position '(75570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue997 :archetype 'static-turret :team 'blue
      :position '(77570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue998 :archetype 'static-turret :team 'blue
      :position '(79570 46650 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-blue999 :archetype 'static-turret :team 'blue
      :position '(81570 46650 0)
      :rotation '(0 0 0))

    ;; Team: red | Archetype: capital-ship | Count: 11
    (entity :id 'capital-ship-red :archetype 'capital-ship :team 'red
      :position '(-71920 54248 85970)
      :rotation '(0 -90 0))
    (entity :id 'capital-ship-red10 :archetype 'capital-ship :team 'red
      :position '(-71920 -22763 81420)
      :rotation '(0 -90 0))
    (entity :id 'capital-ship-red11 :archetype 'capital-ship :team 'red
      :position '(-71920 23300 84980)
      :rotation '(0 -90 0))
    (entity :id 'capital-ship-red2 :archetype 'capital-ship :team 'red
      :position '(-102650 11110 29800)
      :rotation '(0 -90 0))
    (entity :id 'capital-ship-red3 :archetype 'capital-ship :team 'red
      :position '(-94460 49370 29800)
      :rotation '(0 -90 0))
    (entity :id 'capital-ship-red4 :archetype 'capital-ship :team 'red
      :position '(-92130 -24870 29800)
      :rotation '(0 -90 0))
    (entity :id 'capital-ship-red5 :archetype 'capital-ship :team 'red
      :position '(-75190 -53430 29800)
      :rotation '(0 -90 0))
    (entity :id 'capital-ship-red6 :archetype 'capital-ship :team 'red
      :position '(-71920 -25250 59020)
      :rotation '(0 -90 0))
    (entity :id 'capital-ship-red7 :archetype 'capital-ship :team 'red
      :position '(-71920 23300 59020)
      :rotation '(0 -90 0))
    (entity :id 'capital-ship-red8 :archetype 'capital-ship :team 'red
      :position '(-71920 53238 59020)
      :rotation '(0 -90 0))
    (entity :id 'capital-ship-red9 :archetype 'capital-ship :team 'red
      :position '(-71920 -1513 59020)
      :rotation '(0 -90 0))

    ;; Team: red | Archetype: static-turret | Count: 243
    (entity :id 'static-turret-red :archetype 'static-turret :team 'red
      :position '(-57800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red10 :archetype 'static-turret :team 'red
      :position '(-37800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red100 :archetype 'static-turret :team 'red
      :position '(-57800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red101 :archetype 'static-turret :team 'red
      :position '(-27800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red102 :archetype 'static-turret :team 'red
      :position '(-29800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red103 :archetype 'static-turret :team 'red
      :position '(-31800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red104 :archetype 'static-turret :team 'red
      :position '(-33800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red105 :archetype 'static-turret :team 'red
      :position '(-35800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red106 :archetype 'static-turret :team 'red
      :position '(-37800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red107 :archetype 'static-turret :team 'red
      :position '(-39800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red108 :archetype 'static-turret :team 'red
      :position '(-41800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red109 :archetype 'static-turret :team 'red
      :position '(-43800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red11 :archetype 'static-turret :team 'red
      :position '(-39800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red110 :archetype 'static-turret :team 'red
      :position '(-45800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red111 :archetype 'static-turret :team 'red
      :position '(-47800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red112 :archetype 'static-turret :team 'red
      :position '(-49800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red113 :archetype 'static-turret :team 'red
      :position '(-51800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red114 :archetype 'static-turret :team 'red
      :position '(-53800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red115 :archetype 'static-turret :team 'red
      :position '(-55800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red116 :archetype 'static-turret :team 'red
      :position '(-57800 34790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red117 :archetype 'static-turret :team 'red
      :position '(-27800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red118 :archetype 'static-turret :team 'red
      :position '(-29800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red119 :archetype 'static-turret :team 'red
      :position '(-31800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red12 :archetype 'static-turret :team 'red
      :position '(-41800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red120 :archetype 'static-turret :team 'red
      :position '(-33800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red121 :archetype 'static-turret :team 'red
      :position '(-35800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red122 :archetype 'static-turret :team 'red
      :position '(-37800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red123 :archetype 'static-turret :team 'red
      :position '(-39800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red124 :archetype 'static-turret :team 'red
      :position '(-41800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red125 :archetype 'static-turret :team 'red
      :position '(-43800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red126 :archetype 'static-turret :team 'red
      :position '(-45800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red127 :archetype 'static-turret :team 'red
      :position '(-47800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red128 :archetype 'static-turret :team 'red
      :position '(-49800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red129 :archetype 'static-turret :team 'red
      :position '(-51800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red13 :archetype 'static-turret :team 'red
      :position '(-43800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red130 :archetype 'static-turret :team 'red
      :position '(-53800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red131 :archetype 'static-turret :team 'red
      :position '(-55800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red132 :archetype 'static-turret :team 'red
      :position '(-57800 32790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red133 :archetype 'static-turret :team 'red
      :position '(-27800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red134 :archetype 'static-turret :team 'red
      :position '(-29800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red135 :archetype 'static-turret :team 'red
      :position '(-31800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red136 :archetype 'static-turret :team 'red
      :position '(-33800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red137 :archetype 'static-turret :team 'red
      :position '(-35800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red138 :archetype 'static-turret :team 'red
      :position '(-37800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red139 :archetype 'static-turret :team 'red
      :position '(-39800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red14 :archetype 'static-turret :team 'red
      :position '(-45800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red140 :archetype 'static-turret :team 'red
      :position '(-41800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red141 :archetype 'static-turret :team 'red
      :position '(-43800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red142 :archetype 'static-turret :team 'red
      :position '(-45800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red143 :archetype 'static-turret :team 'red
      :position '(-47800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red144 :archetype 'static-turret :team 'red
      :position '(-49800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red145 :archetype 'static-turret :team 'red
      :position '(-51800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red146 :archetype 'static-turret :team 'red
      :position '(-53800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red147 :archetype 'static-turret :team 'red
      :position '(-55800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red148 :archetype 'static-turret :team 'red
      :position '(-57800 30790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red149 :archetype 'static-turret :team 'red
      :position '(-27800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red15 :archetype 'static-turret :team 'red
      :position '(-47800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red150 :archetype 'static-turret :team 'red
      :position '(-29800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red151 :archetype 'static-turret :team 'red
      :position '(-31800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red152 :archetype 'static-turret :team 'red
      :position '(-33800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red153 :archetype 'static-turret :team 'red
      :position '(-35800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red154 :archetype 'static-turret :team 'red
      :position '(-37800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red155 :archetype 'static-turret :team 'red
      :position '(-39800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red156 :archetype 'static-turret :team 'red
      :position '(-41800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red157 :archetype 'static-turret :team 'red
      :position '(-43800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red158 :archetype 'static-turret :team 'red
      :position '(-45800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red159 :archetype 'static-turret :team 'red
      :position '(-47800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red16 :archetype 'static-turret :team 'red
      :position '(-49800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red160 :archetype 'static-turret :team 'red
      :position '(-49800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red161 :archetype 'static-turret :team 'red
      :position '(-51800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red162 :archetype 'static-turret :team 'red
      :position '(-53800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red163 :archetype 'static-turret :team 'red
      :position '(-55800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red164 :archetype 'static-turret :team 'red
      :position '(-57800 28790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red165 :archetype 'static-turret :team 'red
      :position '(-27800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red166 :archetype 'static-turret :team 'red
      :position '(-29800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red167 :archetype 'static-turret :team 'red
      :position '(-31800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red168 :archetype 'static-turret :team 'red
      :position '(-33800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red169 :archetype 'static-turret :team 'red
      :position '(-35800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red17 :archetype 'static-turret :team 'red
      :position '(-51800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red170 :archetype 'static-turret :team 'red
      :position '(-37800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red171 :archetype 'static-turret :team 'red
      :position '(-39800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red172 :archetype 'static-turret :team 'red
      :position '(-41800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red173 :archetype 'static-turret :team 'red
      :position '(-43800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red174 :archetype 'static-turret :team 'red
      :position '(-45800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red175 :archetype 'static-turret :team 'red
      :position '(-47800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red176 :archetype 'static-turret :team 'red
      :position '(-49800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red177 :archetype 'static-turret :team 'red
      :position '(-51800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red178 :archetype 'static-turret :team 'red
      :position '(-53800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red179 :archetype 'static-turret :team 'red
      :position '(-55800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red18 :archetype 'static-turret :team 'red
      :position '(-53800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red180 :archetype 'static-turret :team 'red
      :position '(-57800 26790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red181 :archetype 'static-turret :team 'red
      :position '(-27800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red182 :archetype 'static-turret :team 'red
      :position '(-29800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red183 :archetype 'static-turret :team 'red
      :position '(-31800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red184 :archetype 'static-turret :team 'red
      :position '(-33800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red185 :archetype 'static-turret :team 'red
      :position '(-35800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red186 :archetype 'static-turret :team 'red
      :position '(-37800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red187 :archetype 'static-turret :team 'red
      :position '(-39800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red188 :archetype 'static-turret :team 'red
      :position '(-41800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red189 :archetype 'static-turret :team 'red
      :position '(-43800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red19 :archetype 'static-turret :team 'red
      :position '(-55800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red190 :archetype 'static-turret :team 'red
      :position '(-45800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red191 :archetype 'static-turret :team 'red
      :position '(-47800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red192 :archetype 'static-turret :team 'red
      :position '(-49800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red193 :archetype 'static-turret :team 'red
      :position '(-51800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red194 :archetype 'static-turret :team 'red
      :position '(-53800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red195 :archetype 'static-turret :team 'red
      :position '(-55800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red196 :archetype 'static-turret :team 'red
      :position '(-57800 24790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red197 :archetype 'static-turret :team 'red
      :position '(-27800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red198 :archetype 'static-turret :team 'red
      :position '(-29800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red199 :archetype 'static-turret :team 'red
      :position '(-31800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red2 :archetype 'static-turret :team 'red
      :position '(-53800 48790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red20 :archetype 'static-turret :team 'red
      :position '(-57800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red200 :archetype 'static-turret :team 'red
      :position '(-33800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red201 :archetype 'static-turret :team 'red
      :position '(-35800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red202 :archetype 'static-turret :team 'red
      :position '(-37800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red203 :archetype 'static-turret :team 'red
      :position '(-39800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red204 :archetype 'static-turret :team 'red
      :position '(-41800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red205 :archetype 'static-turret :team 'red
      :position '(-43800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red206 :archetype 'static-turret :team 'red
      :position '(-45800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red207 :archetype 'static-turret :team 'red
      :position '(-47800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red208 :archetype 'static-turret :team 'red
      :position '(-49800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red209 :archetype 'static-turret :team 'red
      :position '(-51800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red21 :archetype 'static-turret :team 'red
      :position '(-27800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red210 :archetype 'static-turret :team 'red
      :position '(-53800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red211 :archetype 'static-turret :team 'red
      :position '(-55800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red212 :archetype 'static-turret :team 'red
      :position '(-57800 22790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red213 :archetype 'static-turret :team 'red
      :position '(-27800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red214 :archetype 'static-turret :team 'red
      :position '(-29800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red215 :archetype 'static-turret :team 'red
      :position '(-31800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red216 :archetype 'static-turret :team 'red
      :position '(-33800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red217 :archetype 'static-turret :team 'red
      :position '(-35800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red218 :archetype 'static-turret :team 'red
      :position '(-37800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red219 :archetype 'static-turret :team 'red
      :position '(-39800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red22 :archetype 'static-turret :team 'red
      :position '(-29800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red220 :archetype 'static-turret :team 'red
      :position '(-41800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red221 :archetype 'static-turret :team 'red
      :position '(-43800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red222 :archetype 'static-turret :team 'red
      :position '(-45800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red223 :archetype 'static-turret :team 'red
      :position '(-47800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red224 :archetype 'static-turret :team 'red
      :position '(-49800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red225 :archetype 'static-turret :team 'red
      :position '(-51800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red226 :archetype 'static-turret :team 'red
      :position '(-53800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red227 :archetype 'static-turret :team 'red
      :position '(-55800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red228 :archetype 'static-turret :team 'red
      :position '(-57800 20790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red229 :archetype 'static-turret :team 'red
      :position '(-27800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red23 :archetype 'static-turret :team 'red
      :position '(-31800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red230 :archetype 'static-turret :team 'red
      :position '(-29800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red231 :archetype 'static-turret :team 'red
      :position '(-31800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red232 :archetype 'static-turret :team 'red
      :position '(-33800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red233 :archetype 'static-turret :team 'red
      :position '(-35800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red234 :archetype 'static-turret :team 'red
      :position '(-37800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red235 :archetype 'static-turret :team 'red
      :position '(-39800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red236 :archetype 'static-turret :team 'red
      :position '(-41800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red237 :archetype 'static-turret :team 'red
      :position '(-43800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red238 :archetype 'static-turret :team 'red
      :position '(-45800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red239 :archetype 'static-turret :team 'red
      :position '(-47800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red24 :archetype 'static-turret :team 'red
      :position '(-33800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red240 :archetype 'static-turret :team 'red
      :position '(-49800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red241 :archetype 'static-turret :team 'red
      :position '(-51800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red242 :archetype 'static-turret :team 'red
      :position '(-53800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red243 :archetype 'static-turret :team 'red
      :position '(-55800 18790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red25 :archetype 'static-turret :team 'red
      :position '(-35800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red26 :archetype 'static-turret :team 'red
      :position '(-37800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red27 :archetype 'static-turret :team 'red
      :position '(-39800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red28 :archetype 'static-turret :team 'red
      :position '(-41800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red29 :archetype 'static-turret :team 'red
      :position '(-43800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red3 :archetype 'static-turret :team 'red
      :position '(-55800 48790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red30 :archetype 'static-turret :team 'red
      :position '(-45800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red31 :archetype 'static-turret :team 'red
      :position '(-47800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red32 :archetype 'static-turret :team 'red
      :position '(-49800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red33 :archetype 'static-turret :team 'red
      :position '(-51800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red34 :archetype 'static-turret :team 'red
      :position '(-53800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red35 :archetype 'static-turret :team 'red
      :position '(-55800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red36 :archetype 'static-turret :team 'red
      :position '(-57800 44790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red37 :archetype 'static-turret :team 'red
      :position '(-27800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red38 :archetype 'static-turret :team 'red
      :position '(-29800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red39 :archetype 'static-turret :team 'red
      :position '(-31800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red4 :archetype 'static-turret :team 'red
      :position '(-57800 48790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red40 :archetype 'static-turret :team 'red
      :position '(-33800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red41 :archetype 'static-turret :team 'red
      :position '(-35800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red42 :archetype 'static-turret :team 'red
      :position '(-37800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red43 :archetype 'static-turret :team 'red
      :position '(-39800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red44 :archetype 'static-turret :team 'red
      :position '(-41800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red45 :archetype 'static-turret :team 'red
      :position '(-43800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red46 :archetype 'static-turret :team 'red
      :position '(-45800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red47 :archetype 'static-turret :team 'red
      :position '(-47800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red48 :archetype 'static-turret :team 'red
      :position '(-49800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red49 :archetype 'static-turret :team 'red
      :position '(-51800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red5 :archetype 'static-turret :team 'red
      :position '(-27800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red50 :archetype 'static-turret :team 'red
      :position '(-53800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red51 :archetype 'static-turret :team 'red
      :position '(-55800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red52 :archetype 'static-turret :team 'red
      :position '(-57800 42790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red53 :archetype 'static-turret :team 'red
      :position '(-27800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red54 :archetype 'static-turret :team 'red
      :position '(-29800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red55 :archetype 'static-turret :team 'red
      :position '(-31800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red56 :archetype 'static-turret :team 'red
      :position '(-33800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red57 :archetype 'static-turret :team 'red
      :position '(-35800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red58 :archetype 'static-turret :team 'red
      :position '(-37800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red59 :archetype 'static-turret :team 'red
      :position '(-39800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red6 :archetype 'static-turret :team 'red
      :position '(-29800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red60 :archetype 'static-turret :team 'red
      :position '(-41800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red61 :archetype 'static-turret :team 'red
      :position '(-43800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red62 :archetype 'static-turret :team 'red
      :position '(-45800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red63 :archetype 'static-turret :team 'red
      :position '(-47800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red64 :archetype 'static-turret :team 'red
      :position '(-49800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red65 :archetype 'static-turret :team 'red
      :position '(-51800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red66 :archetype 'static-turret :team 'red
      :position '(-53800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red67 :archetype 'static-turret :team 'red
      :position '(-55800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red68 :archetype 'static-turret :team 'red
      :position '(-57800 40790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red69 :archetype 'static-turret :team 'red
      :position '(-27800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red7 :archetype 'static-turret :team 'red
      :position '(-31800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red70 :archetype 'static-turret :team 'red
      :position '(-29800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red71 :archetype 'static-turret :team 'red
      :position '(-31800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red72 :archetype 'static-turret :team 'red
      :position '(-33800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red73 :archetype 'static-turret :team 'red
      :position '(-35800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red74 :archetype 'static-turret :team 'red
      :position '(-37800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red75 :archetype 'static-turret :team 'red
      :position '(-39800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red76 :archetype 'static-turret :team 'red
      :position '(-41800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red77 :archetype 'static-turret :team 'red
      :position '(-43800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red78 :archetype 'static-turret :team 'red
      :position '(-45800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red79 :archetype 'static-turret :team 'red
      :position '(-47800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red8 :archetype 'static-turret :team 'red
      :position '(-33800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red80 :archetype 'static-turret :team 'red
      :position '(-49800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red81 :archetype 'static-turret :team 'red
      :position '(-51800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red82 :archetype 'static-turret :team 'red
      :position '(-53800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red83 :archetype 'static-turret :team 'red
      :position '(-55800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red84 :archetype 'static-turret :team 'red
      :position '(-57800 38790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red85 :archetype 'static-turret :team 'red
      :position '(-27800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red86 :archetype 'static-turret :team 'red
      :position '(-29800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red87 :archetype 'static-turret :team 'red
      :position '(-31800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red88 :archetype 'static-turret :team 'red
      :position '(-33800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red89 :archetype 'static-turret :team 'red
      :position '(-35800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red9 :archetype 'static-turret :team 'red
      :position '(-35800 46790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red90 :archetype 'static-turret :team 'red
      :position '(-37800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red91 :archetype 'static-turret :team 'red
      :position '(-39800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red92 :archetype 'static-turret :team 'red
      :position '(-41800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red93 :archetype 'static-turret :team 'red
      :position '(-43800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red94 :archetype 'static-turret :team 'red
      :position '(-45800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red95 :archetype 'static-turret :team 'red
      :position '(-47800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red96 :archetype 'static-turret :team 'red
      :position '(-49800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red97 :archetype 'static-turret :team 'red
      :position '(-51800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red98 :archetype 'static-turret :team 'red
      :position '(-53800 36790 0)
      :rotation '(0 0 0))
    (entity :id 'static-turret-red99 :archetype 'static-turret :team 'red
      :position '(-55800 36790 0)
      :rotation '(0 0 0))))
