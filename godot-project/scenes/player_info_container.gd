class_name PlayerInfoContainer
extends PanelContainer

@onready var name_title_label: Label = $GridContainer/NameTitleLabel
@onready var clock_label: Label = $GridContainer/ClockLabel
@onready var health_label: Label = $GridContainer/HealthLabel
@onready var health_bar: ProgressBar = $GridContainer/HealthBar
@onready var mana_label: Label = $GridContainer/ManaLabel
@onready var mana_bar: ProgressBar = $GridContainer/ManaBar
@onready var strength_label: Label = $GridContainer/StrengthLabel
@onready var dexterity_label: Label = $GridContainer/DexterityLabel
@onready var constitution_label: Label = $GridContainer/ConstitutionLabel
@onready var agility_label: Label = $GridContainer/AgilityLabel
@onready var intelligence_label: Label = $GridContainer/IntelligenceLabel
@onready var power_label: Label = $GridContainer/PowerLabel
@onready var gold_label: Label = $GridContainer/GoldLabel
@onready var hit_label: Label = $GridContainer/HitLabel
@onready var damage_label: Label = $GridContainer/DamageLabel
@onready var defense_label: Label = $GridContainer/DefenseLabel
@onready var armor_label: Label = $GridContainer/ArmorLabel
@onready var speed_label: Label = $GridContainer/SpeedLabel
@onready var level_label: Label = $GridContainer/LevelLabel
@onready var carry_label: Label = $GridContainer/CarryLabel
@onready var hunger_label: Label = $GridContainer/HungerLabel
@onready var poison_label: Label = $GridContainer/PoisonLabel
@onready var disease_label: Label = $GridContainer/DiseaseLabel
@onready var movement_label: Label = $GridContainer/MovementLabel


var player : Main.Player
var game_time : Dictionary

func _ready() -> void:
	name_title_label.text = ""


func set_player(p_player : Main.Player) -> void:
	player = p_player
	update_ui()


func update_time(p_time : Dictionary) -> void:
	game_time = p_time


func update_ui() -> void:
	if player.name.is_empty():
		visible = false
		return

	visible = true
	name_title_label.text = "%s, %s" % [player.name, player.title]
	health_label.text = "HP: %2d/%2d" % [player.hit_points, player.max_hit_points]
	mana_label.text = "MP: %2d/%2d" % [player.mana, player.max_mana]
	strength_label.text = "Str: %2d/%2d" % [player.strength, player.max_strength]
	dexterity_label.text = "Dex: %2d/%2d" % [player.dex, player.max_dex]
	constitution_label.text = "Con: %2d/%2d" % [player.con, player.max_con]
	agility_label.text = "Agi: %2d/%2d" % [player.agi, player.max_agi]
	intelligence_label.text = "Int: %2d/%2d" % [player.iq, player.max_iq]
	power_label.text = "Pow: %2d/%2d" % [player.power, player.max_power]
	gold_label.text = "Au:  %d" % [player.cash]

	clock_label.text = game_time.get("clock", "")
	health_bar.max_value = player.max_hit_points
	health_bar.value = player.hit_points
	mana_bar.max_value = player.max_mana
	mana_bar.value = player.mana
	hit_label.text = "Hit: %3d" % [player.hit]
	damage_label.text = "Dmg: %3d" % [player.dmg]
	defense_label.text = "Def: %3d" % [player.defense]
	armor_label.text = "Arm: %3d" % [player.armor]
	speed_label.text = "Spd: %6.2f" % [5.0 / player.speed]
	level_label.text = "Level: %d/%d" % [player.level, player.xp]
	carry_label.text = ("Carry: %d/%4d" % [player.item_weight, player.max_weight]).rpad(16)
	hunger_label.text = player.hunger
	poison_label.text = "Poisoned" if player.is_poisoned else "Vigorous"
	disease_label.text = "Diseased" if player.is_poisoned else "Healthy"
	movement_label.text = player.movement
