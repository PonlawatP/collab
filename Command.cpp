#include "Command.hpp"
#include "Type/VecType.hpp"
#include "Type/MatrixType.hpp"
#include "Type/ArrType.hpp"
#include "Render/Matrix.hpp"

namespace CppProject
{
	void WriteVarType(QDataStream& stream, VarType value)
	{
		qint32 type = (qint32)value.GetType();
		stream << type;

		switch (type)
		{
			case REAL_t:
				stream << value.Real();
				break;

			case INTEGER_t:
				stream << (qint64)value.Int();
				break;

			case BOOLEAN_t:
				stream << value.Bool();
				break;

			case STRING_t:
				stream << value.Str().QStr();
				break;

			case VECTOR_t:
			{
				const VecType& vec = value.Vec();
				stream << vec.x << vec.y << vec.z << vec.w << (quint8)vec.size;
				break;
			}

			case MATRIX_t:
			{
				const MatrixType& mat = value.Mat();
				for (int i = 0; i < 16; i++)
					stream << mat.matrix.m[i];
				break;
			}

			case ARRAY_t:
			{
				const ArrType& arr = value.Arr();
				stream << (qint32)arr.Size();
				for (IntType i = 0; i < arr.Size(); i++)
					WriteVarType(stream, arr.Value(i));
				break;
			}

			default:
				// UNDEFINED_t (or a *_REF_t, which is never the terminal value of a Command)
				break;
		}
	}

	VarType ReadVarType(QDataStream& stream)
	{
		qint32 type = UNDEFINED_t;
		stream >> type;

		switch (type)
		{
			case REAL_t:
			{
				RealType rl = 0.0;
				stream >> rl;
				return VarType(rl);
			}

			case INTEGER_t:
			{
				qint64 in = 0;
				stream >> in;
				return VarType((IntType)in);
			}

			case BOOLEAN_t:
			{
				BoolType bl = false;
				stream >> bl;
				return VarType(bl);
			}

			case STRING_t:
			{
				QString str;
				stream >> str;
				return VarType(StringType(str));
			}

			case VECTOR_t:
			{
				VecType vec;
				quint8 size = 4;
				stream >> vec.x >> vec.y >> vec.z >> vec.w >> size;
				vec.size = size;
				return VarType(vec);
			}

			case MATRIX_t:
			{
				Matrix matrix;
				for (int i = 0; i < 16; i++)
					stream >> matrix.m[i];
				return VarType(MatrixType(matrix));
			}

			case ARRAY_t:
			{
				qint32 count = 0;
				stream >> count;
				ArrType arr;
				for (qint32 i = 0; i < count; i++)
					arr.Append(ReadVarType(stream));
				return VarType(arr);
			}

			default:
				return VarType();
		}
	}

	void Command::Write(QDataStream& stream) const
	{
		stream << (qint64)peerId << (qint64)instanceId << (qint64)subAssetId << (qint64)memberId << (qint64)sequence;
		WriteVarType(stream, saveId);
		WriteVarType(stream, value);
	}

	Command Command::Read(QDataStream& stream)
	{
		Command cmd;
		qint64 peerId = 0, instanceId = 0, subAssetId = 0, memberId = 0, sequence = 0;
		stream >> peerId >> instanceId >> subAssetId >> memberId >> sequence;
		cmd.peerId = peerId;
		cmd.instanceId = instanceId;
		cmd.subAssetId = subAssetId;
		cmd.memberId = memberId;
		cmd.sequence = sequence;
		cmd.saveId = ReadVarType(stream);
		cmd.value = ReadVarType(stream);
		return cmd;
	}
}
