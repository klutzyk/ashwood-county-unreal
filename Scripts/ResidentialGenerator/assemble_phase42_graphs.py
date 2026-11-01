"""Build BPIR patch; baseline functions come from the live, checkpointed Blueprint."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[2]
out = Path(__file__).parent
baseline = (root / 'Saved/ResidentialGenerator/Checkpoints/ResumePhase42_20261031/BlueprintDump/bpir.txt').read_text(encoding='utf-8-sig')
def graph(name):
    match = re.search(r'entry function ' + name + r'\([^\n]*\{.*?\n\}', baseline, re.S)
    assert match, name
    text = re.sub(r' @\(-?\d+, -?\d+\)', '', match.group())
    return (text
            .replace('E_ResidentialLayoutStyle::OpenPlan', '3')
            .replace('E_ResidentialLayoutStyle::CentralHallway', '4')
            .replace('E_ResidentialFurnishingDensity::Normal', '1'))
patch = (out / 'phase42_interiors.bpir').read_text()
components = ['DoorFrame', 'Entrance', 'Furniture_Sofa', 'Furniture_SofaClassic',
 'Furniture_Armchair', 'Furniture_ArmchairClassic', 'Furniture_CoffeeTable', 'Furniture_Bed',
 'Furniture_Bedside', 'Furniture_Wardrobe', 'Furniture_WardrobeClassic',
 'Furniture_DresserModern', 'Furniture_DresserClassic', 'Furniture_DiningTable',
 'Furniture_DiningTableClassic', 'Furniture_DiningChair', 'Furniture_DiningChairClassic',
 'Furniture_Bathtub', 'Furniture_Toilet', 'Kitchen_BaseStandard', 'Kitchen_BaseLeft']
patch += '\nentry function KitchenRunClear(float X, float Y) -> bool {\n'
patch += '    %center = call MakeVector(X: $X, Y: $Y, Z: 100)\n    %extent = call MakeVector(X: 164, Y: 90, Z: 79)\n'
for i, comp in enumerate(components):
    patch += f'    %hit{i} = call KitchenInstancesInBox(Component: ${comp}, Center: %center, Extent: %extent)\n'
    patch += f'    branch(%hit{i}) [true -> @blocked, false -> @check{i}]\n@check{i}:\n'
patch += '    return true\n@blocked:\n    return false\n}\n'
clear = graph('ClearGeneratedInteriors')
new = ['Kitchen_BaseStandard', 'Kitchen_BaseLeft', 'Kitchen_PlinthStandard', 'Kitchen_PlinthLeft', 'Kitchen_Counter', 'Kitchen_Sink', 'Kitchen_Upper']
clear = clear.replace('    return', ''.join(f'    call TransactObject(Object: ${c})\n    call ClearInstances(Target: ${c})\n' for c in new) + '    return')
patch += '\n' + clear
workflow = (out / 'phase4_1_workflow.bpir').read_text().replace('call ApplyInteriorFinish()', 'call ApplyInteriorFinish()\n    call ApplyPhase42Materials()').replace('call BuildFloors()', 'call BuildRoomFloors()').replace('Generated Phase 4.1 interior; logical rooms:', 'Phase 4.2 cabinetry (sink asset pending); logical rooms:')
patch += '\n' + workflow
light = graph('ConfigureGeneratedLight').replace('NewTemperature: 2950', 'NewTemperature: 4200')
# Preserve intensity control while reducing its effective output by 25 percent.
light = light.replace('    call SetIntensity(', '    %balancedIntensity = call Multiply_DoubleDouble(A: $Intensity, B: 0.75)\n    call SetIntensity(').replace('NewIntensity: $Intensity', 'NewIntensity: %balancedIntensity')
patch += '\n' + light
# Place dining first so the kitchen obstacle query sees all its chairs.
patch += '\n' + graph('FurnishCombinedPublic').replace('A: %n1.X, B: 180', 'A: %n1.X, B: 150')
room = graph('FurnishRoom')
room = room.replace('    call FurnishKitchen(Room: $Room)\n    call FurnishCompactDining(Room: $Room)', '    call FurnishCompactDining(Room: $Room)\n    call FurnishKitchen(Room: $Room)')
room = room.replace('    call FurnishCompactDining(Room: $Room)\n    call FurnishKitchen(Room: $Room)', '    %kitchenSize = call BreakVector(InVec: %n0.Size_6_A050281547D25A85B4448B81EF60035D)\n    %diningFits = call GreaterEqual_DoubleDouble(A: %kitchenSize.Y, B: 600)\n    branch(%diningFits) [true -> @kitchenDining, false -> @kitchenCabinets]\n@kitchenDining:\n    call FurnishCompactDining(Room: $Room)\n@kitchenCabinets:\n    call FurnishKitchen(Room: $Room)')
patch += '\n' + room
source = (out / 'phase4_1_furnishing.bpir').read_text()
for name in ('BuildNewLayoutShell', 'BuildShell'):
    patch += '\n' + graph(name)
for name in ('FurnishBedroomExtras', 'FurnishDiningAtX'):
    match = re.search(r'entry function ' + name + r'\([^\n]*\{.*?\n\}', source, re.S)
    assert match, name
    patch += '\n' + match.group()
living = re.search(r'entry function FurnishLivingAtX\([^\n]*\{.*?\n\}', source, re.S).group()
living = living.replace('B: 160)', 'B: 180)')
for side in ('back', 'front'):
    label = side + 'Details'
    living = living.replace('@' + label + ':\n    call PlaceThemedArmchair', '@' + label + ':\n    %' + side + 'ChairFits = call GreaterEqual_DoubleDouble(A: %size.X, B: 560)\n    branch(%' + side + 'ChairFits) [true -> @' + side + 'Chair, false -> @' + side + 'Side]\n@' + side + 'Chair:\n    call PlaceThemedArmchair')
    needle = 'Yaw: %chairYaw)\n    call Place(Component: $Furniture_Bedside'
    living = living.replace(needle, 'Yaw: %chairYaw)\n@' + side + 'Side:\n    call Place(Component: $Furniture_Bedside', 1)
living = living.replace('    %frontFacade =', '    %entryDelta = call Subtract_DoubleDouble(A: %chairX, B: $EntranceCenterX)\n    %entryDistance = call Abs(A: %entryDelta)\n    %entryClear = call GreaterEqual_DoubleDouble(A: %entryDistance, B: 125)\n    %notFront = call NotEqual_DoubleDouble(A: %min.Y, B: 0)\n    %chairEntrySafe = call BooleanOR(A: %entryClear, B: %notFront)\n    %frontFacade =')
for side in ('back', 'front'):
    living = living.replace('    branch(%' + side + 'ChairFits)', '    %' + side + 'ChairSafe = call BooleanAND(A: %' + side + 'ChairFits, B: %chairEntrySafe)\n    branch(%' + side + 'ChairSafe)')
patch += '\n' + living
(out / 'phase42_patch.bpir').write_text(patch)
print(len(patch), 'bytes')

